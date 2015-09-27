/*
 * ShamirClient.hh
 *
 *  Created on: 06.06.2012
 *      Author: Dominik Oepen
 */

#ifndef SHAMIR_CLIENT_HH_
#define SHAMIR_CLIENT_HH_

#include <string>
#include <queue>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashtable.hh>
#include <click/packet.hh>
#include "elements/brn/brnelement.hh"

#include <openssl/bn.h>

using namespace std;

CLICK_DECLS

class ShamirClient : public BRNElement {
public:
	ShamirClient();
	~ShamirClient();

	const char *class_name() const { return "ShamirClient"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

    /** @brief Broadcast a request to reconstruct the key */
    int send_request();

	void add_handlers();

    /** @brief The global modulus used for all threshold computation */
    BIGNUM *_modulus;
    unsigned int _threshold;
	int _debug;
private:
    EtherAddress _me;
    BN_CTX *_bn_ctx;
    //unsigned int _num_shares;
    HashTable<uint32_t, BIGNUM *> _received_shares;

    int store_reply(Packet *p);
    void combine();
};

CLICK_ENDDECLS
#endif /* SHAMIR_CLIENT_HH_ */
