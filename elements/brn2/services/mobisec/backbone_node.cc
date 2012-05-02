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

backbone_node::backbone_node()
	: _debug(false)
{
	BRNElement::init(); // what for??
}

backbone_node::~backbone_node() {
}

int backbone_node::configure(Vector<String> &conf, ErrorHandler *errh) {
	String _protocol_type_str;

	if (cp_va_kparse(conf, this, errh,
		"PROTOCOL_TYPE", cpkP, cpString, &_protocol_type_str,
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

int backbone_node::initialize(ErrorHandler* errh) {
}

void backbone_node::push(int port, Packet *p) {
	if(port == 0) {
		BRN_DEBUG("Got kdp-server reply... yey");
		handle_kdp_reply(p);
	} else {
		p->kill();
	}
}

void backbone_node::snd_kdp_req() {
	WritablePacket *p = kdp::kdp_request_msg();

	kdp_req *req = (kdp_req *)p;
	req->req_id = req_id++; // Problem: speicherung von req_id

	BRN_DEBUG("Sending KDP-Request...");
	output(0).push(p);
}

void backbone_node::handle_kdp_reply(Packet *p) {

}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */
static String handler_triggered_request(Element *e, void *thunk) {
	backbone_node *bn = (backbone_node *)e;
	bn->snd_kdp_req();
	return String();
}

void backbone_node::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("snd_kdp_request", handler_triggered_request, NULL);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(backbone_node)
