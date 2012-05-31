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
	: _debug(false),
	  _timer(this),
	  keyman()
{
	BRNElement::init();
}

backbone_node::~backbone_node() {
}

int backbone_node::configure(Vector<String> &conf, ErrorHandler *errh) {
	String _protocol_type_str;

	if (cp_va_kparse(conf, this, errh,
		"NODEID", cpkP+cpkM, cpElement, &_me,
		"PROTOCOL_TYPE", cpkP+cpkM, cpString, &_protocol_type_str,
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

int backbone_node::initialize(ErrorHandler* errh) {
	req_id = 7;

	BRN_DEBUG("Backbone node initialized");
	return 0;
}

void backbone_node::push(int port, Packet *p) {
	if(port == 0) {
		BRN_DEBUG("kdp reply received");
		handle_kdp_reply(p);
	} else {
		p->kill();
	}
}

void backbone_node::run_timer(Timer* ) {

}

void backbone_node::snd_kdp_req() {
	WritablePacket *p = kdp::kdp_request_msg();

	// Enrich packet with information.
	kdp_req *req = (kdp_req *)p->data();
	req->node_id = *(_me->getServiceAddress());
	req->req_id = req_id++;

	BRN_DEBUG("Sending KDP-Request...");
	output(0).push(p);
}

void backbone_node::handle_kdp_reply(Packet *p) {
	crypto_ctrl_data *hdr = (crypto_ctrl_data *)p->data();
	const unsigned char *payload = &(p->data()[sizeof(crypto_ctrl_data)]);

	// Store crypto control data
	keyman.set_ctrl_data(hdr);
	BRN_INFO("card: %d; seed_len: %d", keyman.get_ctrl_data()->cardinality, keyman.get_ctrl_data()->seed_len);

	// Store crypto material
	if (keyman.get_ctrl_data()->seed_len > 0) {
		keyman.set_seed(payload);

		BRN_DEBUG("Constructing key list");
		keyman.constr_keylist_cli_driv();
	} else
		;// todo: store the received key list

	p->kill();
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
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(backbone_node)
