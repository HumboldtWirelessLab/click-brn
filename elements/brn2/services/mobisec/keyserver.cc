/*
 * keyserver.cc
 *
 *  Created on: 27.04.2012
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

#include "keyserver.hh"
#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

keyserver::keyserver()
	: _debug(false),
	  _timer(this),
	  keyman()
{
	BRNElement::init();
}

keyserver::~keyserver() {
}

int keyserver::configure(Vector<String> &conf, ErrorHandler *errh) {
	String _protocol_type_str;

	if (cp_va_kparse(conf, this, errh,
		"PROTOCOL_TYPE", cpkP+cpkM, cpString, &_protocol_type_str,
		"KEY_LIST_CARDINALITY", cpkP+cpkM, cpInteger, &_key_list_cardinality,
		"KEY_TIMEOUT", cpkP+cpkM, cpInteger, &_key_timeout,
		"START", cpkP+cpkM, cpInteger, &_start_time,
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

int keyserver::initialize(ErrorHandler* errh) {
	if (_protocol_type == SERVER_DRIVEN)
		keyman.gen_crypto_srv_driv();
	else if (_protocol_type == CLIENT_DRIVEN)
		keyman.gen_crypto_cli_driv();

	// set crypto parameters made by configuration
	keyman.set_validity_start_time(_start_time);
	keyman.set_cardinality(_key_list_cardinality);
	keyman.set_key_timeout(_key_timeout);

	// todo: temporarily doing the call here (has to be done by a timed function)
	if (_protocol_type == SERVER_DRIVEN)
		keyman.constr_keylist_srv_driv();
	else if (_protocol_type == CLIENT_DRIVEN)
		keyman.constr_keylist_cli_driv();

	// Set timer for key setting
	_timer.initialize(this);
	_timer.schedule_after_msec(_start_time);

	BRN_DEBUG("Key server initialized");
	return 0;
}

void keyserver::push(int port, Packet *p) {
	if(port==0) {
		BRN_DEBUG("kdp request received");
		handle_kdp_req(p);
	} else {
		BRN_DEBUG("Oops. Wrong port.");
		p->kill();
	}
}

void keyserver::run_timer(Timer* ) {

	_timer.reschedule_after_msec(_key_timeout);

	/*
	 * Todo: 2 Fälle:
	 * 1. Wenn nicht der vorletzte Schlüssel ansteht, dann einfach passenden Schlüssel an erster Stelle einsetzen,
	 * parallele Schlüssel entfernen.
	 * 2. Sonst, Schlüsselliste konstruieren und zwei Schlüssel parallel einsetzen
	 */

	BRN_DEBUG("Installing new keys: ");
	keyman.install_key_on_phy(_wepencap, _wepdecap);
}

void keyserver::handle_kdp_req(Packet *p) {
	/*
	 * Further Work: protocol checks; here good place to control replay attacks
	 */
	kdp_req *req = (kdp_req *)p->data();
	BRN_DEBUG("Received kdp req %d from %s", (req->req_id), (req->node_id).unparse().c_str());
	p->kill();

	crypto_ctrl_data *hdr = keyman.get_ctrl_data();
	const unsigned char *payload;

	if(_protocol_type == CLIENT_DRIVEN) {
		payload = keyman.get_seed();

	} else if (_protocol_type == SERVER_DRIVEN) {
		//todo: payload = keyman.keylist;
	}

	WritablePacket *reply;
	reply = kdp::kdp_reply_msg(hdr, payload);

	BRN_DEBUG("sending kdp reply");
	output(0).push(reply);
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(keyserver)
