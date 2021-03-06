/*
 * keyserver.cc
 *
 *  Created on: 27.04.2012
 *      Author: kuehne@informatik.hu-berlin.de
 *      Description: The MobiSEC key server is one of the three parties
 *      in a MobiSEC mash network, leaving the backbone node and the clients.
 *      The key server uses the key distribution protocol to communicate
 *      with the backbone node to deliver the key material.
 */


#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/timestamp.hh>
#include <click/handlercall.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"

#include "keyserver.hh"
#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

KeyServer::KeyServer() :
  _me(NULL),
  _wepencap(NULL),
  _wepdecap(NULL),
  _tls(NULL),
  _start_time(0),
  session_timer(session_trigger, this),
  epoch_timer(epoch_trigger, this),
  start_flag(false),
  keyman(),
  BUF_keyman(),
  _key_list_cardinality(0),
  _key_timeout(0),
  bb_status(false),
  bb_join_cnt(0),
  key_inst_cnt(0),
  cnt_bb_entrypoints(0),
  cnt_bb_exitpoints(0)
{
	BRNElement::init();
}

KeyServer::~KeyServer() {
}

int KeyServer::configure(Vector<String> &conf, ErrorHandler *errh) {
	String _protocol_type_str;

	if (cp_va_kparse(conf, this, errh,
		"NODEID", cpkP+cpkM, cpElement, &_me,

		"PROTOCOL_TYPE", cpkP+cpkM, cpString, &_protocol_type_str,
		"KEY_LIST_CARDINALITY", cpkP+cpkM, cpInteger, &_key_list_cardinality,
		"KEY_TIMEOUT", cpkP+cpkM, cpInteger, &_key_timeout,

		"START", cpkP+cpkM, cpInteger, &_start_time,

		"WEPENCAP", cpkP+cpkM, cpElement, &_wepencap,
		"WEPDECAP", cpkP+cpkM, cpElement, &_wepdecap,

		"TLS", cpkP+cpkM, cpElement, &_tls,

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

int KeyServer::initialize(ErrorHandler *) {
	// Needed for bootstrapping of prepare_new_epoch
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

	// Set statistical variables
	bb_status = false;
	bb_join_cnt = 0;
	key_inst_cnt = 0;
	cnt_bb_entrypoints = 0;
	cnt_bb_exitpoints = 0;

	prepare_new_epoch();
	epoch_timer.initialize(this);	// Set timer to coordinate epoch keylists
	jmp_next_epoch(); 				// We do the triggering manually here

	// Set timer to coordinate session keys
	session_timer.initialize(this);
	jmp_next_session();

	BRN_DEBUG("Key server initialized");
	return 0;
}

void KeyServer::push(int port, Packet *p) {
	if(port==0) {
		handle_kdp_req(p);
	} else {
		BRN_DEBUG("Oops. Wrong port.");
		p->kill();
	}
}

void KeyServer::handle_kdp_req(Packet *p) {
	const kdp_req *req = reinterpret_cast<const kdp_req *>(p->data());
	BRN_DEBUG("Received kdp req %d from %s", (req->req_id), (req->node_id).unparse().c_str());

	// Todo: check restrictions, limits, constrains: Is it possible to be out of a epoch range?
	if(req->req_id < 0) {
    BRN_ERROR("req_id %d seams not correct (from %s)",(req->req_id), (req->node_id).unparse().c_str());
    p->kill();
    return;
  }

	EtherAddress dst_addr = (req->node_id);

	// For now, no further use for *p and *req.
	p->kill();

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
	KeyManagement *curr_keyman;
	if (epoch_begin < now && now <= thrashold) {
		curr_keyman = &keyman;
	} else if (thrashold < now && now <= epoch_end+keylist_livetime) {
		curr_keyman = &BUF_keyman;
	} else {
		BRN_ERROR("keyserver seams to be out of epoch! begin:%d end:%d now:%d", epoch_begin, epoch_end, now);
		return;
	}

	crypto_ctrl_data *hdr = curr_keyman->get_ctrl_data();

	const unsigned char *payload = NULL;
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
	reply = KDP::kdp_reply_msg(_protocol_type, hdr, payload);

	// Set ethernet address of client
	BRNPacketAnno::set_ether_anno(reply, *(_me->getServiceAddress()), dst_addr, ETHERTYPE_BRN);

	BRN_DEBUG("sending kdp reply");
	output(0).push(reply);

	// After KDP execution server side shutdown finishes communication.
	// Todo: This is cross layer action, and with this handler we have no control
	// about current connection! It would be better to shutdown by etheraddress.
	// HandlerCall::call_read(_tls, "shutdown", NULL);

	free(keylist_string);
}

/*
 * *******************************************************
 *         Time-dependent tasks
 * *******************************************************
 */
void KeyServer::jmp_next_session() {
	BRN_DEBUG("Installing new keys: ");
	if (keyman.install_key_on_phy(_wepencap, _wepdecap)) {
			key_inst_cnt++;
	}

	// Find out session we are in since keylist timestamp. Every session has the length of _key_timeout.
	// (Note: Round down is done implicitly through integer arithmetic.
	int offset = ((Timestamp::now().msecval()-keyman.get_validity_start_time()) / _key_timeout)*_key_timeout + _key_timeout;

	session_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + offset));
}

void KeyServer::jmp_next_epoch() {
	// Switch to new epoch (copy new epoch data from BUF_keyman to keyman)
	if ( keyman.set_ctrl_data( BUF_keyman.get_ctrl_data() ) ) {

		// As key server we have to take care of seed, which is requested by clients
		if (_protocol_type == CLIENT_DRIVEN) {
			keyman.set_seed(BUF_keyman.get_seed());
		}

		keyman.install_keylist( BUF_keyman.get_keylist() );

		if(!bb_status) {
			cnt_bb_entrypoints++;
			bb_status = true;
		}
		bb_join_cnt++;
		BRN_DEBUG("Switched to new epoch");
	} else {
		if(bb_status) {
			cnt_bb_exitpoints++;
			bb_status = false;
		}
		BRN_DEBUG("Jump to next epoch failed due to wrong control data.");
	}

	prepare_new_epoch();

	// Set timer for the next epoch jump
	int anticipation = 200;
	int keylist_livetime = _key_timeout*BUF_keyman.get_cardinality();
	epoch_timer.schedule_at(Timestamp::make_msec(keyman.get_validity_start_time() + keylist_livetime - anticipation));
}

/*
 * This is a central key server function. In order to provide the security items
 * (time stamp, key list, seed) for the whole network the server not only has to
 * generate the security items but to keep them ready early enough. In this
 * implementation the key server does this right after entering the current epoch.
 */
void KeyServer::prepare_new_epoch() {
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

String KeyServer::stats() {
	  StringAccum sa;

	  sa << "<mobisec node=\"" << BRN_NODE_NAME
			  << "\" node=\"sk1"
			  << "\" time=\"" << Timestamp::now().msecval()
			  << "\" bb_entrypoints=\"" << cnt_bb_entrypoints
			  << "\" bb_exitpoints=\"" << cnt_bb_exitpoints
			  << "\" bb_join_cnt=\"" << bb_join_cnt
			  << "\" key_inst_cnt=\"" << key_inst_cnt << "\" />";

	  return sa.take_string();
}


enum { H_STATS};

static String keyserver_read_param(Element *e, void *thunk) {
	KeyServer *bn = reinterpret_cast<KeyServer *>(e);

	switch ((uintptr_t) thunk) {
	case H_STATS: {
		return bn->stats();
	}
	default:
		return String();
	}
}

void KeyServer::add_handlers() {
	BRNElement::add_handlers();

	add_read_handler("stats", keyserver_read_param, (void *)H_STATS);
}




CLICK_ENDDECLS
ELEMENT_REQUIRES(ns TLS)
EXPORT_ELEMENT(KeyServer)
