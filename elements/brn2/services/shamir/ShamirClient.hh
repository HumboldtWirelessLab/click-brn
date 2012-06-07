/*
 * tls.hh
 *
 *  Created on: 18.04.2012
 *      Author: aureliano
 */

#ifndef SHAMIR_CLIENT_HH_
#define SHAMIR_CLIENT_HH_

#include <string>
#include <queue>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashtable.hh>
#include <click/packet.hh>
#include "elements/brn2/brnelement.hh"

#include <openssl/bn.h>


using namespace std;

CLICK_DECLS

class ShamirClient : public BRNElement {
public:
	ShamirClient();
	~ShamirClient();

	const char *class_name() const { return "ShamirClient"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	void add_handlers();
    BIGNUM *_modulus;
	int _debug;
private:
    BN_CTX *_bn_ctx;
    unsigned int _num_shares;
    HashTable<uint32_t, BIGNUM *> _received_shares;


    int send_request();
    int store_reply(Packet *p);
    BIGNUM *combine();
};

CLICK_ENDDECLS
#endif /* SHAMIR_CLIENT_HH_ */
