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
	  km()
{
	BRNElement::init();
}

keyserver::~keyserver() {
}

int keyserver::configure(Vector<String> &conf, ErrorHandler *errh) {
	String _protocol_type_str;

	if (cp_va_kparse(conf, this, errh,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		"PROTOCOL_TYPE", cpkP, cpString, &_protocol_type_str,
		"KEY_LIST_CARDINALITY", cpkP, cpInteger, &_key_list_cardinality,
		"KEY_TIMEOUT", cpkP, cpInteger, &_key_timeout,
		"START", cpkP, cpInteger, &_start_time,
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
		km.generate_crypto_srv_driv();
	else if (_protocol_type == CLIENT_DRIVEN)
		km.generate_crypto_cli_driv();

	// set cardinality in keymanagement
	km.set_cardinality(_key_list_cardinality);
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

}

void keyserver::handle_kdp_req(Packet *p) {
	/*
	 * todo: protocol checks
	 * here good place to control replay attacks
	 */
	kdp_req *req = (kdp_req *)p->data();
	BRN_DEBUG("Received kdp req %d from %s", (req->req_id), (req->node_id).unparse().c_str());
	p->kill();

	crypto_ctrl_data *hdr = km.get_ctrl_data();
	const unsigned char *payload;

	if(_protocol_type == CLIENT_DRIVEN) {
		payload = km.get_seed();

	} else if (_protocol_type == SERVER_DRIVEN) {
		//todo: payload = km.keylist;
	}

	WritablePacket *reply;
	reply = kdp::kdp_reply_msg(hdr, payload);

	BRN_DEBUG("sending kdp reply");
	output(0).push(reply);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(keyserver)
