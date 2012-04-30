/*
 * kdp.cc
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

#include "kdp.hh"

CLICK_DECLS

kdp::kdp() {
}

kdp::~kdp() {
}

WritablePacket *
kdp::kdp_request_msg() {
	WritablePacket *p = Packet::make(128, NULL, sizeof(struct kdp_req), 32);

	kdp_req req;

	memcpy(p->data(), &req, sizeof(kdp_req));

	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(kdp)
