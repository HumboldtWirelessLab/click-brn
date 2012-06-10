/*
 * backbone_node.cc
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */
#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>

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
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	if (_protocol_type_str == "SERVER-DRIVEN")
		_protocol_type = SERVER_DRIVEN;
	else if (_protocol_type_str == "CLIENT-DRIVEN")
		_protocol_type = CLIENT_DRIVEN;
	else
		return -1;

	return 0;
}

int BACKBONE_NODE::initialize(ErrorHandler* errh) {
	req_id = 7; // Not used, but maybe useful for future work on replay-defense
	tolerance = _key_timeout * 1.5;

	// Configuration determines some crypto parameters
	keyman.set_key_timeout(_key_timeout);

	// Set timer to send first kdp-request
	kdp_timer.initialize(this);
	kdp_timer.schedule_at(Timestamp::make_msec(_start_time));

	session_timer.initialize(this);
	session_timer.schedule_at(Timestamp::make_msec(_start_time));

	epoch_timer.initialize(this);

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
	WritablePacket *p = kdp::kdp_request_msg();

	// Enrich packet with information.
	kdp_req *req = (kdp_req *)p->data();
	req->node_id = *(_me->getServiceAddress());
	req->req_id = req_id++;

	BRN_DEBUG("Sending KDP-Request...");
	output(0).push(p);

	/*
	 * This is our "hope" mechanism:
	 * We expect a reply up to a certain time. Otherwise
	 * we need to send a new request and pray.
	 */
	if (BUF_keyman.get_validity_start_time() < Timestamp::now().msecval())
		kdp_timer.schedule_after_sec(4);
}

void BACKBONE_NODE::handle_kdp_reply(Packet *p) {
	crypto_ctrl_data *hdr = (crypto_ctrl_data *)p->data();
	const unsigned char *payload = &(p->data()[sizeof(crypto_ctrl_data)]);

	// Buffer crypto control data
	BUF_keyman.set_ctrl_data(hdr);
	BRN_INFO("card: %d; seed_len: %d", BUF_keyman.get_ctrl_data()->cardinality, BUF_keyman.get_ctrl_data()->seed_len);

	// Buffer crypto material
	if (_protocol_type == CLIENT_DRIVEN) {
		BUF_keyman.set_seed(payload);

		BRN_DEBUG("Constructing key list");
		BUF_keyman.install_keylist_cli_driv();
	} else if (_protocol_type == SERVER_DRIVEN) {
		;// todo: store the received key list
	}

	// Set timer to jump right into the coming epoch, it's unbelievable close
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
	keyman.install_key_on_phy(_wepencap, _wepdecap);

	// Find out session we are in since keylist timestamp. Every session has the length of _key_timeout.
	// (Note: Round down is done implicitly through integer arithmetic.
	int offset = ((Timestamp::now().msecval()-keyman.get_validity_start_time()) / _key_timeout)*_key_timeout + _key_timeout;

	session_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + offset));
}

void BACKBONE_NODE::jmp_next_epoch() {
	// Switch to new epoch (copy new epoch data from BUF_keyman to keyman)
	keyman.set_ctrl_data( BUF_keyman.get_ctrl_data() );
	keyman.set_seed( BUF_keyman.get_seed() );
	keyman.set_validity_start_time( BUF_keyman.get_validity_start_time() );
	(_protocol_type == SERVER_DRIVEN) ? keyman.install_keylist_srv_driv() // Todo: how to get keylist here???
										:
										keyman.install_keylist_cli_driv();

	BRN_DEBUG("Switched to new epoch");
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */
static String handler_triggered_request(Element *e, void *thunk) {
	BACKBONE_NODE *bn = (BACKBONE_NODE *)e;
	bn->snd_kdp_req();
	return String();
}

void BACKBONE_NODE::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("snd_kdp_request", handler_triggered_request, NULL);
}



CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(BACKBONE_NODE)
