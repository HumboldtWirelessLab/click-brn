/*
 * ShamirServer.hh
 *
 *  Created on: 05.06.2012
 *      Author: Dominik Oepen
 */

#ifndef SHAMIR_SERVER_HH_
#define SHAMIR_SERVER_HH_

#include <string>
#include <queue>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/packet.hh>
#include "elements/brn2/brnelement.hh"

#include <openssl/bn.h>

using namespace std;

CLICK_DECLS

class ShamirServer : public BRNElement {
public:
	ShamirServer();
	~ShamirServer();

	const char *class_name() const { return "ShamirServer"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	void add_handlers();

    BIGNUM *_modulus;
    BIGNUM *_share;
	int _debug;
    unsigned int _share_id;
private:
    EtherAddress _me;
    BN_CTX *_bn_ctx;

    int handle_request(Packet *p);
};

CLICK_ENDDECLS
#endif /* SHAMIR_SERVER_HH_ */
