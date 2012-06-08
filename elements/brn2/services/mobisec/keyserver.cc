/*
 * keyserver.cc
 *
 *  Created on: 27.04.2012
 *      Author: aureliano
 */


#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/timestamp.hh>

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
	  session_timer(session_trigger, this),
	  epoche_timer(epoche_trigger, this),
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
	start_flag = true;

	// Set crypto parameters made by configuration
	keyman.set_validity_start_time(_start_time);
	keyman.set_cardinality(_key_list_cardinality);
	keyman.set_key_timeout(_key_timeout);

	// Set timer to coordinate key and keylist installation.
	session_timer.initialize(this);
	session_timer.schedule_at(Timestamp::make_msec(_start_time));

	// Set timer to build crypto material.
	epoche_timer.initialize(this);
	jmp_next_epoche(); // We do the triggering manually here

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

void keyserver::handle_kdp_req(Packet *p) {
	// Todo: Future Work: protocol checks; here good place to control replay attacks

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

/*
 * *******************************************************
 *         Time-dependent tasks
 * *******************************************************
 */
void keyserver::jmp_next_session() {
	/*
	 * Todo: 2 Fälle:
	 * 1. Wenn nicht der vorletzte Schlüssel ansteht, dann einfach passenden Schlüssel an erster Stelle einsetzen,
	 * parallele Schlüssel entfernen.
	 * 2. Sonst, zwei Schlüssel parallel einsetzen
	 */

	BRN_DEBUG("NEW Installing new keys: ");
	keyman.install_key_on_phy(_wepencap, _wepdecap);

	// Find out time slice we are in since keylist timestamp. Every slice has the length of _key_timeout.
	// (Note: Round down is done implicitly through integer arithmetic.
	int offset = ((Timestamp::now().msecval()-keyman.get_validity_start_time()) / _key_timeout)*_key_timeout + _key_timeout;

	session_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + offset));
}

/*
 * Two jobs have to be done in the last session of an epoche:
 * 1. The keyserver must produce new crypto material before next kdp request come from backbone nodes
 * 2. The keyserver needs to build his own new keylist
 */
void keyserver::jmp_next_epoche() {
	BRN_DEBUG("Preparing new Epoche ...");

	if (_protocol_type == SERVER_DRIVEN)
		keyman.gen_crypto_srv_driv();
	else if (_protocol_type == CLIENT_DRIVEN)
		keyman.gen_crypto_cli_driv();

	BRN_DEBUG("Built crypto material");

	if (_protocol_type == SERVER_DRIVEN)
		keyman.constr_keylist_srv_driv();
	else if (_protocol_type == CLIENT_DRIVEN)
		keyman.constr_keylist_cli_driv();

	BRN_DEBUG("Constructed new keylist");

	int keylist_livetime = _key_timeout*_key_list_cardinality;

	// Omit first induction step for right order
	if(start_flag)
		start_flag = false;
	else
		keyman.set_validity_start_time(keyman.get_validity_start_time() + keylist_livetime);

	int tolerance = 0.5*_key_timeout;
	epoche_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + keylist_livetime - tolerance));
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(keyserver)
