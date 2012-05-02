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

CLICK_DECLS

kdp::kdp() {
}

kdp::~kdp() {
}

// This function used for both server and client driven kdp
WritablePacket * kdp::kdp_request_msg() {
	WritablePacket *p = Packet::make(128, NULL, sizeof(struct kdp_req), 32);

	kdp_req req;

	memcpy(p->data(), &req, sizeof(struct kdp_req));

	return p;
}

WritablePacket *kdp::kdp_reply_msg_cli_driv() {
	//todo: instead of statical infos we have to retrieve infos from KMM
	int seed_len = 150; //sha1
	int cardinality = 4;
	time_t ts = time(NULL);
	int seed = 1337;

	WritablePacket *p = Packet::make(128, NULL, sizeof(struct kdp_reply_header)+seed_len, 32);

	kdp_reply_header hdr = {ts, cardinality, 0, seed_len};

	memcpy(p->data(), &hdr, sizeof(struct kdp_reply_header));
	memcpy(p->data()+sizeof(struct kdp_reply_header), &seed, sizeof(int));

	return p;
}

WritablePacket *kdp::kdp_reply_msg_srv_driv() {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(kdp)
