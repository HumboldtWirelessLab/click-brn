/*
 * KDP.hh -- Key Distribution Protocol
 *
 *  Created on: 27.04.2012
 *      Author: aureliano
 *
 * This module only delivers the protocol messages.
 */

#ifndef KDP_HH_
#define KDP_HH_


#include <string>
#include <time.h>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashmap.hh>
#include "elements/brn/brnelement.hh"

#include "keymanagement.hh"

CLICK_DECLS


enum proto_type {SERVER_DRIVEN, CLIENT_DRIVEN};

struct kdp_req {
	int req_id;
	EtherAddress node_id;
};

class KDP : public BRNElement {
public:
	KDP();
	~KDP();

	const char *class_name() const { return "KDP"; }

	static WritablePacket *kdp_request_msg();

	static WritablePacket *kdp_reply_msg(enum proto_type type, crypto_ctrl_data *hdr, const unsigned char *payload);
private:
};

CLICK_ENDDECLS
#endif /* KDP_HH_ */
