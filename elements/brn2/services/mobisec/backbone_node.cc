/*
 * backbone_node.cc
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
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
		"ASSOCREQ", cpkP+cpkM, cpElement, &_assocreq,
		"AP_Q", cpkP+cpkM, cpElement, &_ap_q,
		"CLIENT_Q", cpkP+cpkM, cpElement, &_client_q,
		"DEVICE_CONTROL_UP", cpkP, cpElement, &_dev_control_up,
		"DEVICE_CONTROL_DOWN", cpkP, cpElement, &_dev_control_down,
		"DEVICE_CONTROL_DOWN2", cpkP, cpElement, &_dev_control_down2,
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
	last_req_try = 0;

	// As mentioned in the MobiSEC-Paper, but defined by myself
	tolerance = _key_timeout * 1.5 + randnum;

	// Time between retries of kdp request transmission
	backoff = _key_timeout * 0.5 + randnum;

	// Configuration determines some crypto parameters
	keyman.set_key_timeout(_key_timeout);

	// Set timer to send first kdp-request
	kdp_timer.initialize(this);

	session_timer.initialize(this);
	session_timer.schedule_at(Timestamp::make_msec(_start_time));

	epoch_timer.initialize(this);

	switch_dev(dev_client);

	// Set stat variables
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

void BACKBONE_NODE::snd_kdp_req() {
	/*
	 * Turn off WEP temporarily, if we can't receive a kdp-reply.
	 * Reason: Packets might be wep-encrypted wrong due to
	 * missing or wrong keys. Therefore wep must be switched off
	 * temporarily.
	 */
	if (Timestamp::now().msecval()-last_req_try < backoff*1.5) {// If request was send a little time ago, retry without wep

		BRN_DEBUG("Retry kdp process...");
		kdp_retry_cnt++;

		switch_dev(dev_client);

		HandlerCall::call_read(_tls, "restart", NULL);

	} else { // If not, then we are about to send our first request for next epoch data
		last_req_try = Timestamp::now().msecval();
	}

	WritablePacket *p = kdp::kdp_request_msg();

	// Enrich packet with information.
	kdp_req *req = (kdp_req *)p->data();
	req->node_id = *(_me->getServiceAddress());
	req->req_id = req_id++;

	BRN_DEBUG("Sending KDP-Request...");
	output(0).push(p);

	/* Begin of "resend mechanism" in case of no response */
	// Todo: Find a reasonable time for backoff
	kdp_timer.schedule_after_msec(backoff);
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
	int anticipation = 200;
	epoch_timer.schedule_at(Timestamp::make_msec(BUF_keyman.get_validity_start_time() - anticipation));

	// Set timer to send next kdp-req
	int keylist_livetime = _key_timeout*BUF_keyman.get_cardinality();
	kdp_timer.reschedule_at(Timestamp::make_msec(BUF_keyman.get_validity_start_time() + keylist_livetime - tolerance));

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

	keyman.set_ctrl_data( BUF_keyman.get_ctrl_data() );

	keyman.install_keylist( BUF_keyman.get_keylist() );

	BRN_DEBUG("Switched to new epoch");
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */

void BACKBONE_NODE::switch_dev(enum dev_type type) {
	String type_str;
	String port;

	switch(type) {
	case dev_ap:
		port = "1";
		type_str = "dev_ap";
		break;
	case dev_client:
		port = "0";
		type_str = "dev_client";
		break;
	default:
		BRN_ERROR("Received wrong arg in switch_dev()");
		return;
	}

	String curr_port = HandlerCall::call_read(_dev_control_up, "switch", NULL);
	if(curr_port == port) {
		BRN_DEBUG("Using same dev: %s", type_str.c_str());
		return;
	} else {
		BRN_DEBUG("Switching device to %s", type_str.c_str());

		// Both devices are functioning although the flow only runs through one device.
		// So when switching the device its queue is possibly overflowed. Thus we
		// have to reset the queues to get a clean start.
		HandlerCall::call_write(_ap_q, "reset", port, NULL);
		HandlerCall::call_write(_client_q, "reset", port, NULL);

		if (port == "1") {
			BRN_DEBUG("Sending disassoc to abandon client status");
			HandlerCall::call_write(_assocreq, "send_disassoc_req", NULL);
		}
	}

	// Tell the Click-Switch to switch and thus let the packet flow go through the
	// other device.
	HandlerCall::call_write(_dev_control_up, "switch", port, NULL);
	HandlerCall::call_write(_dev_control_down, "switch", port, NULL);
	HandlerCall::call_write(_dev_control_down2, "switch", port, NULL);
}


String BACKBONE_NODE::stats() {
	  StringAccum sa;

	  sa << "<mobisec node=\"" << BRN_NODE_NAME
			  << "\" bb_join_cnt=\"" << bb_join_cnt
			  << "\" key_inst_cnt=\"" << key_inst_cnt
			  << "\" kdp_retry_cnt=\"" << kdp_retry_cnt << "\" />";

	  return sa.take_string();
}



enum {H_SND_KDP_REQ, H_STATS};

static String backbone_read_param(Element *e, void *thunk) {
	BACKBONE_NODE *bn = (BACKBONE_NODE *)e;

	switch ((uintptr_t) thunk) {
	case H_SND_KDP_REQ: {
		bn->snd_kdp_req();
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
	add_read_handler("stats", backbone_read_param, (void *)H_STATS);
}



CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(BACKBONE_NODE)
