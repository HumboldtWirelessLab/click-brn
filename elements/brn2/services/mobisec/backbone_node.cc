/*
 * backbone_node.cc
 *
 *  Created on: 28.04.2012
 *      Author: kuehne@informatik.hu-berlin.de
 *      Description: The MobiSEC backbone node is one of the three parties
 *      in a MobiSEC mash network, leaving the key server and the clients.
 *      The backbone node uses the key distribution protocol to communicate
 *      with the key server to acquire the key material.
 *
 *      The subcomponents of the backbone node are: keymanagement, kdp. These
 *      are organized by the backbone node itself.
 *
 *      The keymanagement gets the key material installs it on the physical layer.
 *
 *      The kdp provides the backbone node with kdp network packets.
 */
#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/handlercall.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "kdp.hh"
#include "backbone_node.hh"

#define RETRY_CNT_DOWN 1

CLICK_DECLS

BACKBONE_NODE::BACKBONE_NODE()
	: _debug(false),
	  session_timer(session_trigger, this),
	  epoch_timer(epoch_trigger, this),
	  kdp_timer(kdp_trigger, this),
	  keyman(),
	  BUF_keyman()
{
	BRNElement::init();
}

BACKBONE_NODE::~BACKBONE_NODE() {
}

int BACKBONE_NODE::configure(Vector<String> &conf, ErrorHandler *errh) {
	String _protocol_type_str;

	if (cp_va_kparse(conf, this, errh,
		"NODEID", cpkP+cpkM, cpElement, &_me,
		"PROTOCOL_TYPE", cpkP+cpkM, cpString, &_protocol_type_str,
		"START", cpkP+cpkM, cpInteger, &_start_time,
		"KEY_TIMEOUT", cpkP+cpkM, cpInteger, &_key_timeout,
		"WEPENCAP", cpkP+cpkM, cpElement, &_wepencap,
		"WEPDECAP", cpkP+cpkM, cpElement, &_wepdecap,
		"TLS", cpkP+cpkM, cpElement, &_tls,

		// BEGIN elements to control network stack
		"DEV_CLIENT", cpkP+cpkM, cpElement, &_wifidev_client,
		"AP_Q", cpkP+cpkM, cpElement, &_ap_q,
		"CLIENT_Q", cpkP+cpkM, cpElement, &_client_q,
		"DEVICE_CONTROL_UP", cpkP, cpElement, &_dev_control_up,
		"DEVICE_CONTROL_DOWN", cpkP, cpElement, &_dev_control_down,
		"DEVICE_CONTROL_DOWN2", cpkP, cpElement, &_dev_control_down2,
		// END elements to control network stack

		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	if (_protocol_type_str == "SERVER-DRIVEN")
		_protocol_type = SERVER_DRIVEN;
	else if (_protocol_type_str == "CLIENT-DRIVEN")
		_protocol_type = CLIENT_DRIVEN;
	else {
		return -1;
	}

	BRN_DEBUG("Protocol type: %s", _protocol_type_str.c_str());

	return 0;
}

int BACKBONE_NODE::initialize(ErrorHandler *) {
	// Pseudo randomness as creepy solution for simulation in
	// order to get asynchronous packet transmission
	long randnum = (long)this;
	click_srandom((int)randnum);
	randnum = randnum%1337;
	BRN_DEBUG("random number: %i", randnum);

	req_id = 0;

	// As mentioned in the MobiSEC-Paper, but defined by myself
	tolerance = _key_timeout * 1.5 + randnum;

	// Time between retries of kdp request transmission (Todo: Find a reasonable
	// time for backoff, NOT defined by MobiSEC-paper)
	backoff = _key_timeout * 0.5;
	retry_cnt_down = RETRY_CNT_DOWN;

	// Configuration determines some crypto parameters
	keyman.set_key_timeout(_key_timeout);

	// Set timer to send first kdp-request
	kdp_timer.initialize(this);

	session_timer.initialize(this);
	session_timer.schedule_at(Timestamp::make_msec(_start_time));

	epoch_timer.initialize(this);

	switch_dev(dev_client);

	// Set statistical variables
	bb_join_cnt = 0;
	kdp_retry_cnt = 0;
	key_inst_cnt = 0;

	BRN_DEBUG("Backbone node initialized");
	return 0;
}

void BACKBONE_NODE::push(int port, Packet *p) {
	if(port == 0) {
		BRN_DEBUG("kdp reply received");
		handle_kdp_reply(p);
	} else {
		p->kill();
	}
}

/*
 * To understand the inner workings of snd_kdp_req() and handle_kdp_reply()
 * please consider to read both functions, having in mind, that
 * they also remain on a third component: the timers!
 * The timers are responsible for the correct time-dependent execution, while
 * handle_kdp_reply() might reschedule snd_kdp_req().
 */
void BACKBONE_NODE::snd_kdp_req() {
	/*
	 * Switch to client_dev (fall back), if we can't receive a kdp-reply.
	 * Reason: Packets might be wep-encrypted wrong due to
	 * missing or wrong keys. Therefore we switch back to client_dev
	 */
	if (retry_cnt_down <= 0) {
		BRN_DEBUG("Retry kdp process...");
		kdp_retry_cnt++;

		switch_dev(dev_client);

		HandlerCall::call_read(_tls, "restart", NULL);
	} else {
		retry_cnt_down--;
	}

	WritablePacket *p = kdp::kdp_request_msg();

	// Enrich packet with information.
	kdp_req *req = (kdp_req *)p->data();
	req->node_id = *(_me->getServiceAddress());
	req->req_id = req_id++;

	BRN_DEBUG("Sending KDP-Request...");
	output(0).push(p);

	// Begin of "resend mechanism" in case of no response.
	// This loop will terminate, if we receive our kdp reply (see handle_kdp_reply).
	kdp_timer.schedule_after_msec(backoff + tolerance);
}

void BACKBONE_NODE::handle_kdp_reply(Packet *p) {
	crypto_ctrl_data *hdr = (crypto_ctrl_data *)p->data();
	const unsigned char *payload = &(p->data()[sizeof(crypto_ctrl_data)]);

	// Make a scummy check for packet repetition
	if (hdr->timestamp == BUF_keyman.get_validity_start_time()) {
		BRN_DEBUG("kdp-reply already received");
		return;
	}

	// Buffer crypto control data
	if (!BUF_keyman.set_ctrl_data(hdr))
		return;

	BRN_INFO("card: %d; key_len: %d", BUF_keyman.get_ctrl_data()->cardinality, BUF_keyman.get_ctrl_data()->key_len);

	// We got some key material, switch to backbone network
	switch_dev(dev_ap);

	// Buffer crypto material
	if (_protocol_type == CLIENT_DRIVEN) {
		data_t *seed = (data_t *)payload;
		BRN_DEBUG("Constructing and installing key list");
		BUF_keyman.install_keylist_cli_driv(seed);

	} else if (_protocol_type == SERVER_DRIVEN) {
		data_t *keylist_string = (data_t *)payload;
		BRN_DEBUG("Installing key list");
		BUF_keyman.install_keylist_srv_driv(keylist_string);
	}

	if(BUF_keyman.get_keylist().size() == BUF_keyman.get_cardinality()) {
		BRN_DEBUG("READY TO JOIN BACKBONE");
		bb_join_cnt++;
	} else {
		BRN_ERROR("KEYLIST ERROR");
	}

	// Set timer to jump right into the coming epoch. If a kdp request was received successfully on
	// the first try, we are very close to enter the new epoche. If it takes longer to install
	// the keys than we probably missed the entry point in time and have to wait until the actual
	// session passes.
	int anticipation = 200; // This is important to prevent a dangerous switch while installing new epoch data.
	epoch_timer.schedule_at(Timestamp::make_msec(BUF_keyman.get_validity_start_time() - anticipation));

	// Set timer to send next kdp-req
	int keylist_livetime = _key_timeout*BUF_keyman.get_cardinality();
	kdp_timer.reschedule_at(Timestamp::make_msec(BUF_keyman.get_validity_start_time() + keylist_livetime - tolerance));
	retry_cnt_down = RETRY_CNT_DOWN; // reset retry countdown

	p->kill();
}

/*
 * *******************************************************
 *         Time-dependent tasks
 * *******************************************************
 */
void BACKBONE_NODE::jmp_next_session() {
	BRN_DEBUG("Installing new keys: ");
	if (keyman.install_key_on_phy(_wepencap, _wepdecap)) {
		key_inst_cnt++;
	}

	// Find out session we are in since keylist timestamp. Every session has the length of _key_timeout.
	// (Note: Round down is done implicitly through integer arithmetic.
	int offset = ((Timestamp::now().msecval()-keyman.get_validity_start_time()) / _key_timeout)*_key_timeout + _key_timeout;

	session_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + offset));
}

void BACKBONE_NODE::jmp_next_epoch() {
	// Switch to new epoch (copy new epoch data from BUF_keyman to keyman)

	if ( keyman.set_ctrl_data( BUF_keyman.get_ctrl_data() ) ) {
		keyman.install_keylist( BUF_keyman.get_keylist() );
		BRN_DEBUG("Switched to new epoch");
	} else {
		BRN_DEBUG("Jump to next epoch failed due to wrong control data.");
	}
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */

void BACKBONE_NODE::switch_dev(enum dev_type type) {
	String type_str;
	String network_stack_nr;

	switch(type) {
	case dev_ap:
		network_stack_nr = "1";
		type_str = "dev_ap";
		break;
	case dev_client:
		network_stack_nr = "0";
		type_str = "dev_client";
		break;
	default:
		BRN_ERROR("Received wrong arg in switch_dev()");
		return;
	}

	String curr_port = HandlerCall::call_read(_dev_control_up, "switch", NULL);
	if(curr_port == network_stack_nr) {
		BRN_DEBUG("Using same dev: %s", type_str.c_str());
		return;
	} else {
		BRN_DEBUG("Switching device to %s", type_str.c_str());

		// Both devices are functioning although the flow only runs through one device.
		// So when switching the device its queue is possibly overflowed. Thus we
		// have to reset the queues to get a clean start.
		HandlerCall::call_write(_ap_q, "reset", network_stack_nr, NULL);
		HandlerCall::call_write(_client_q, "reset", network_stack_nr, NULL);
	}

	if (network_stack_nr == "1") {
		BRN_DEBUG("Sending disassoc to abandon client status");
		HandlerCall::call_write(_wifidev_client, "disassoc", NULL);
	}

	// Tell the Click-Switch to switch and thus let the packet flow go through the
	// other device.
	HandlerCall::call_write(_dev_control_up, "switch", network_stack_nr, NULL);
	HandlerCall::call_write(_dev_control_down, "switch", network_stack_nr, NULL);
	HandlerCall::call_write(_dev_control_down2, "switch", network_stack_nr, NULL);

	if (network_stack_nr == "0") {
		BRN_DEBUG("Sending assoc to next ap");
		HandlerCall::call_write(_wifidev_client, "do_assoc", NULL);
	}

	BRN_DEBUG("Switching completed.");
}


String BACKBONE_NODE::stats() {
	  StringAccum sa;

	  sa << "<mobisec node=\"" << BRN_NODE_NAME
			  << "\" bb_join_cnt=\"" << bb_join_cnt
			  << "\" key_inst_cnt=\"" << key_inst_cnt
			  << "\" kdp_retry_cnt=\"" << kdp_retry_cnt << "\" />";

	  return sa.take_string();
}

void BACKBONE_NODE::reset_key_material() {
	// Set the validity start time of keys to 0 in order to make them expire.
	keyman.set_validity_start_time(0);
	BRN_DEBUG("reset key material");
}



enum {H_SND_KDP_REQ, H_STATS, H_RST_KEYS};

static String backbone_read_param(Element *e, void *thunk) {
	BACKBONE_NODE *bn = (BACKBONE_NODE *)e;

	switch ((uintptr_t) thunk) {
	case H_SND_KDP_REQ: {
		bn->snd_kdp_req();
		return String();
	}
	case H_RST_KEYS: {
		bn->reset_key_material();
		return String();
	}
	case H_STATS: {
		return bn->stats();
	}
	default:
		return String();
	}
}

void BACKBONE_NODE::add_handlers() {
	BRNElement::add_handlers();

	add_read_handler("snd_kdp_request", backbone_read_param, (void *)H_SND_KDP_REQ);
	add_read_handler("reset_key_material", backbone_read_param, (void *)H_RST_KEYS);
	add_read_handler("stats", backbone_read_param, (void *)H_STATS);
}



CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(BACKBONE_NODE)
