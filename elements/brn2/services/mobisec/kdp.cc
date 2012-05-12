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

WritablePacket *kdp::kdp_reply_msg(crypto_ctrl_data *hdr, const unsigned char *payload) {

	int data_len = (hdr->seed_len > 0) ? hdr->seed_len : (hdr->key_len*hdr->cardinality);

	WritablePacket *p = Packet::make(128, NULL, sizeof(crypto_ctrl_data)+data_len, 32);

	memcpy(p->data(), hdr, sizeof(crypto_ctrl_data));
	memcpy(&p->data()[sizeof(crypto_ctrl_data)], payload, data_len);

	return p;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(kdp)
