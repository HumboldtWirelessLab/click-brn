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
	  BUF_keyman()
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

	BRN_DEBUG("Protocol type: %s", _protocol_type_str.c_str());

	return 0;
}

int KEYSERVER::initialize(ErrorHandler *) {
	start_flag = true;

	// Configuration determines some crypto parameters
	keyman.set_validity_start_time(_start_time);
	keyman.set_cardinality(_key_list_cardinality);
	keyman.set_keylen(5);
	keyman.set_seedlen(20);//160 bit for sha1  make less than 20 byte
	keyman.set_key_timeout(_key_timeout);

	/*
	 * Todo: Vielleicht eleganter, wenn ich einen "copy constructor" benutze
	 *
	 * Grund: In prepare_new_epoch() können sich sonst Fehler einschleichen,
	 * weil ich nicht alle nötigen Daten in BUF_keyman übernehme. BUF_keyman
	 * ist jedoch wichtig, weil es die Quelle für keyman darstellt.
	 *
	 * Vorgehen: http://en.wikipedia.org/wiki/Rule_of_three_%28C%2B%2B_programming%29
	 *
	 * BUF_keyman = keyman;
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

	// Todo: check restrictions, limits, constrains: Is it possible to be out of a epoch range?
	if(req->req_id < 0) {BRN_ERROR("req_id %d seams not correct (from %s)",(req->req_id), (req->node_id).unparse().c_str()); return;}

	int keylist_livetime = _key_timeout*keyman.get_cardinality();
	int now = Timestamp::now().msecval();
	int epoch_begin = keyman.get_validity_start_time() - _key_timeout * 1.5;
	int epoch_end = epoch_begin + keylist_livetime;
	int thrashold = epoch_end - _key_timeout * 1.5;

	/*
	 * Two cases can happen for a reply:
	 * 1. We are in the current epoch, thus a client gets the CURRENT key material.
	 * 2. We are on the edge to a new epoch, thus the client gets the BRAND NEW key material.
	 */
	keymanagement *curr_keyman;
	if (epoch_begin < now && now <= thrashold) {
		curr_keyman = &keyman;
	} else if (thrashold < now && now <= epoch_end+keylist_livetime) {
		curr_keyman = &BUF_keyman;
	} else {
		BRN_ERROR("keyserver seams to be out of epoch! begin:%d end:%d now:%d", epoch_begin, epoch_end, now);
		return;
	}

	crypto_ctrl_data *hdr = curr_keyman->get_ctrl_data();

	const unsigned char *payload;
	data_t *keylist_string = NULL;

	if(_protocol_type == CLIENT_DRIVEN) {
		payload = curr_keyman->get_seed();
	} else if (_protocol_type == SERVER_DRIVEN) {
		keylist_string = curr_keyman->get_keylist_string();
		payload = keylist_string;
	}

	if(!payload) {
		BRN_ERROR("NO PAYLOAD! ABORT");
		return;
	}

	WritablePacket *reply;
	reply = kdp::kdp_reply_msg(hdr, payload);

	BRN_DEBUG("sending kdp reply");
	output(0).push(reply);

	free(keylist_string);
}

/*
 * *******************************************************
 *         Time-dependent tasks
 * *******************************************************
 */
void KEYSERVER::jmp_next_session() {
	BRN_DEBUG("Installing new keys: ");
	keyman.install_key_on_phy(_wepencap, _wepdecap);

	// Find out session we are in since keylist timestamp. Every session has the length of _key_timeout.
	// (Note: Round down is done implicitly through integer arithmetic.
	int offset = ((Timestamp::now().msecval()-keyman.get_validity_start_time()) / _key_timeout)*_key_timeout + _key_timeout;

	session_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + offset));
}

void KEYSERVER::jmp_next_epoch() {
	// Switch to new epoch (copy new epoch data from BUF_keyman to keyman)
	keyman.set_ctrl_data( BUF_keyman.get_ctrl_data() );
	if (BUF_keyman.get_seed())
		keyman.set_seed( BUF_keyman.get_seed() );
	keyman.install_keylist( BUF_keyman.get_keylist() );

	BRN_DEBUG("Switched to new epoch");

	prepare_new_epoch();

	// Set timer for the next epoch jump
	int anticipation = 200;
	int keylist_livetime = _key_timeout*BUF_keyman.get_cardinality();
	epoch_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + keylist_livetime - anticipation));
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
	// First induction step is a little bit tricky. Some arrangements have to be done manually.
	if(start_flag) {
		start_flag = false;
		BUF_keyman.set_ctrl_data(keyman.get_ctrl_data());
	} else {
		int keylist_livetime = _key_timeout*BUF_keyman.get_cardinality();
		BUF_keyman.set_validity_start_time(keyman.get_validity_start_time() + keylist_livetime);
	}

	(_protocol_type == SERVER_DRIVEN) ? BUF_keyman.gen_keylist()
										:
										BUF_keyman.gen_seeded_keylist();

	BRN_DEBUG("Prepared next epoch");
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(KEYSERVER)
