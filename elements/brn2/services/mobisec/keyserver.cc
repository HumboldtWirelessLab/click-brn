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

KEYSERVER::KEYSERVER()
	: _debug(false),
	  session_timer(session_trigger, this),
	  epoch_timer(epoch_trigger, this),
	  keyman(),
	  TMP_keyman()
{
	BRNElement::init();
}

KEYSERVER::~KEYSERVER() {
}

int KEYSERVER::configure(Vector<String> &conf, ErrorHandler *errh) {
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

int KEYSERVER::initialize(ErrorHandler* errh) {
	start_flag = true;

	// Configuration determines some crypto parameters
	keyman.set_validity_start_time(_start_time);
	keyman.set_cardinality(_key_list_cardinality);
	keyman.set_key_timeout(_key_timeout);

	/*
	 * Todo: Vielleicht eleganter, wenn ich einen "copy constructor" benutze
	 *
	 * Grund: In prepare_new_epoch() können sich sonst Fehler einschleichen,
	 * weil ich nicht alle nötigen Daten in TMP_keyman übernehme. TMP_keyman
	 * ist jedoch wichtig, weil es die Quelle für keyman darstellt.
	 *
	 * Vorgehen: http://en.wikipedia.org/wiki/Rule_of_three_%28C%2B%2B_programming%29
	 *
	 * TMP_keyman = keyman;
	 */


	prepare_new_epoch();
	epoch_timer.initialize(this);	// Set timer to coordinate epoch keylists
	jmp_next_epoch(); 				// We do the triggering manually here

	// Set timer to coordinate session keys
	session_timer.initialize(this);
	session_timer.schedule_at(Timestamp::make_msec(_start_time));

	BRN_DEBUG("Key server initialized");
	return 0;
}

void KEYSERVER::push(int port, Packet *p) {
	if(port==0) {
		BRN_DEBUG("kdp request received");
		handle_kdp_req(p);
	} else {
		BRN_DEBUG("Oops. Wrong port.");
		p->kill();
	}
}

void KEYSERVER::handle_kdp_req(Packet *p) {
	// Todo: Future Work: protocol checks; here good place to control replay attacks

	kdp_req *req = (kdp_req *)p->data();
	BRN_DEBUG("Received kdp req %d from %s", (req->req_id), (req->node_id).unparse().c_str());
	p->kill();

	// TODO: Hier muss entschieden werden, aus welcher Epoche die Informationen
	// verschickt werden sollen!

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
void KEYSERVER::jmp_next_session() {
	/*
	 * Todo: 2 Fälle:
	 * 1. Wenn nicht der vorletzte Schlüssel ansteht, dann einfach passenden Schlüssel an erster Stelle einsetzen,
	 * parallele Schlüssel entfernen.
	 * 2. Sonst, zwei Schlüssel parallel einsetzen
	 */

	BRN_DEBUG("Installing new keys: ");
	keyman.install_key_on_phy(_wepencap, _wepdecap);

	// Find out session we are in since keylist timestamp. Every session has the length of _key_timeout.
	// (Note: Round down is done implicitly through integer arithmetic.
	int offset = ((Timestamp::now().msecval()-keyman.get_validity_start_time()) / _key_timeout)*_key_timeout + _key_timeout;

	session_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + offset));
}

void KEYSERVER::jmp_next_epoch() {
	// Copy new epoch data from TMP_keyman to keyman
	keyman.set_ctrl_data( TMP_keyman.get_ctrl_data() );
	keyman.set_seed( TMP_keyman.get_seed() );
	keyman.set_validity_start_time( TMP_keyman.get_validity_start_time() );
	(_protocol_type == SERVER_DRIVEN) ? keyman.install_keylist_srv_driv() // Todo: how to get keylist here???
										:
										keyman.install_keylist_cli_driv();

	BRN_DEBUG("Switched to new epoch");

	prepare_new_epoch();

	// Set timer for the next epoch jump
	int tolerance = 0.5*_key_timeout;
	int keylist_livetime = _key_timeout*_key_list_cardinality;
	epoch_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + keylist_livetime - tolerance));
}

/*
 * This function allows asynchronous handling of "jumping to the next epoch"
 * and preparing the data for the NEXT epoch. This idea reflects the inticacies
 * of network communication, where on the one hand everybody operates
 * with the same data, on the other hand every node has to think of his future
 * communication and therefore assure that the data for the next epoch is
 * present in time.
 */
void KEYSERVER::prepare_new_epoch() {
	(_protocol_type == SERVER_DRIVEN) ? TMP_keyman.gen_keylist()
										:
										TMP_keyman.gen_seed();

	int keylist_livetime = _key_timeout*_key_list_cardinality;

	// First induction step is a little bit tricky. Some arrangements have to be done.
	if(start_flag) {
		start_flag = false;
		TMP_keyman.set_validity_start_time(keyman.get_validity_start_time());
		TMP_keyman.set_cardinality(keyman.get_cardinality());
	} else {
		TMP_keyman.set_validity_start_time(keyman.get_validity_start_time() + keylist_livetime);
	}

	BRN_DEBUG("Prepared next epoch");
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(KEYSERVER)
