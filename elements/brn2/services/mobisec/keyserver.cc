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

CLICK_DECLS

keyserver::keyserver()
	: _debug(false),
	  _timer(this)
{
	BRNElement::init(); // what for??
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
		"INTERVAL", cpkP, cpInteger, &_interval,
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
}

void keyserver::push(int port, Packet *p) {
	// As a server we handle client requests
	//handle_kpd_req(p);
}

void keyserver::run_timer(Timer* ) {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(keyserver)
