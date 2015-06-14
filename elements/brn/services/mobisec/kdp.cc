/*
 * KDP.cc
 *
 *  Created on: 27.04.2012
 *      Author: aureliano
 *      Description: Provides packets for KDP.
 */


#include <time.h>

#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"

#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

KDP::KDP()
{
	BRNElement::init();
}

KDP::~KDP() {
}

/*
 * *******************************************************
 *               Packet Generation Functions
 * *******************************************************
 */

// This function used for both server and client driven KDP
WritablePacket * KDP::kdp_request_msg() {
	WritablePacket *p = Packet::make(128, NULL, sizeof(kdp_req), 32);

	kdp_req req;

	memcpy(p->data(), &req, sizeof(struct kdp_req));

	return p;
}

WritablePacket *KDP::kdp_reply_msg(enum proto_type type, crypto_ctrl_data *hdr, const unsigned char *payload) {

	int data_len = (type == CLIENT_DRIVEN) ? hdr->seed_len : (hdr->key_len*hdr->cardinality);

	WritablePacket *p = Packet::make(128, NULL, sizeof(crypto_ctrl_data)+data_len, 32);

	memcpy(p->data(), hdr, sizeof(crypto_ctrl_data));
	memcpy(&p->data()[sizeof(crypto_ctrl_data)], payload, data_len);

	return p;
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns TLS)
ELEMENT_PROVIDES(KDP)
