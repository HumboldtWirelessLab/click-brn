/*
 * kdp.cc
 *
 *  Created on: 27.04.2012
 *      Author: aureliano
 */


#include <time.h>

#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

kdp::kdp()
{
	BRNElement::init();
}

kdp::~kdp() {
}

/*
 * *******************************************************
 *               Packet Generation Functions
 * *******************************************************
 */

// This function used for both server and client driven kdp
WritablePacket * kdp::kdp_request_msg() {
	WritablePacket *p = Packet::make(128, NULL, sizeof(kdp_req), 32);

	kdp_req req;

	memcpy(p->data(), &req, sizeof(struct kdp_req));

	return p;
}

WritablePacket *kdp::kdp_reply_msg_cli_driv(crypto_info *ci) {

	WritablePacket *p = Packet::make(128, NULL, sizeof(kdp_reply_header)+ci->seed_len, 32);

	kdp_reply_header hdr = {ci->timestamp, ci->cardinality, ci->key_len, ci->seed_len};

	memcpy(p->data(), &hdr, sizeof(kdp_reply_header));
	memcpy(&p->data()[sizeof(kdp_reply_header)], ci->data, ci->seed_len);

	return p;
}

WritablePacket *kdp::kdp_reply_msg_srv_driv(crypto_info *ci) {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(kdp)
