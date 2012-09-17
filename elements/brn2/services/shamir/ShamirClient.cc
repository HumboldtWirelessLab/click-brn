/*
 * ShamirServer.cc
 *
 *  Created on: 08.05.2012
 *      Author: Dominik Oepen
 *
 */

#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/packet.hh>
#include <click/error.hh>

#include <iostream>
#include <sstream>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "ShamirClient.hh"
#include "Shamir.hh"

#include <openssl/bn.h>

CLICK_DECLS

ShamirClient::ShamirClient()
	: _debug(false)
{
	BRNElement::init(); // what for??
}

ShamirClient::~ShamirClient() {

}


int ShamirClient::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh,
        "ETHERADDRESS"m cpkP, cpEthernetAddress, &_me,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	return 0;
}

int ShamirClient::initialize(ErrorHandler *) {
    return 0;
}

void ShamirClient::push(int port, Packet *p) {

	if (port == 0) { // data from network
		BRN_DEBUG("Received Shamir reply");
		this->store_reply(p);
	} else {
		BRN_DEBUG("port %d: oops !!", port, p->length());
		p->kill();
	}
}

int ShamirClient::send_request() {
    String s = "HOTSAUCE";
    WritablePacket *p = Packet::make(128, NULL, sizeof("HOTSAUCE"), 32);
    memcpy(p->data(), &s, sizeof(s));
    BRN_DEBUG("Sending Shamir request");
    output(0).push(p);
    return 0;
}
/*
 * *******************************************************
 *               private functions
 * *******************************************************
 */

void ShamirClient::combine() {
    BIGNUM *tmp, *product, *result;
    uint32_t current_id;

    /*TODO: Perform some sanity checks */

    result = BN_new();
    BN_zero(result);

    for (HashTable<uint32_t, BIGNUM *>::iterator it = _received_shares.begin(); it; ++it) {
        current_id = it.key();
        int current_lagrange = 1;

        /* Compute the Lagrange coeffient for the base polynomial at x=0
         * l_{i,0} = \Prod{j}{j-1} \forall j \in A, j \neq i where A is the set of
         * cooperating nodes */
        for (HashTable<uint32_t, BIGNUM *>::iterator it2 = _received_shares.begin(); it2; ++it2) {
            if (current_id == it2.key())
                continue;
            else
                /*FIXME: Division in Z_p is not necessarily feasible (if p is not prime and phi(p) is unknown) */
                current_lagrange *= it2.key() / (it2.key() - current_id);
        }
        /* Multiply each share with its corresponding Lagrange coeffiecient */
        tmp = BN_new(); //TODO: Use BN_CTX_get
        product = BN_new();
        if (current_lagrange < 0) {
            current_lagrange *= -1;
            BN_set_word(tmp, current_lagrange);
            BN_set_negative(tmp, 1);
        } else {
            BN_set_word(tmp, current_lagrange);
        }
        BN_mod_mul(product, tmp, it.value(), _modulus, _bn_ctx);
        BN_mod_add(result, result, product, _modulus, _bn_ctx);
        BN_free(tmp);
        BN_free(product);
    }

    BRN_DEBUG(BN_bn2hex(result));

    return;
}

int ShamirClient::store_reply(Packet *p) {

    uint32_t share_id, share_length;
    const unsigned char *data = p->data();
    BIGNUM *bn = NULL;

    share_id = *(uint32_t*) data;
    share_length = *(uint32_t*) (data + sizeof(uint32_t));

    if (share_length > MAX_SHARESIZE) {
        BRN_DEBUG("Received data is bigger than MAX_SHARESIZE");
        p->kill();
        return -1;
    }

    bn = BN_bin2bn(data + 2*sizeof(uint32_t), share_length, NULL);

    if (!bn) {
        BRN_DEBUG("Failed to parse reply");
        p->kill();
        return -1;
    } else {
        _received_shares.set(share_id, bn);
    }

    //Check if we have enough shares to combine the secret
    if (_received_shares.size() >= _threshold)
        combine();

    p->kill();
    return 0;
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */

enum {
    H_MODULUS,
    H_THRESHOLD,
    H_ACTIVE,
};

static String read_param(Element *e, void *thunk) {
	ShamirClient *shamir_client = (ShamirClient *)e;
    char *c = NULL;
    StringAccum s;

    switch((intptr_t) thunk) {
    case H_MODULUS:
        {
            c = BN_bn2hex(shamir_client->_modulus);
            s << c;
            free(c);
            break;
        }
    default:
        {
//            BRN_DEBUG("Invalid call to write handler");
        }
    }

    return s.take_string();
}

static int write_param(const String &in_s, Element *e, void *vparam,
            ErrorHandler *errh) {
    ShamirClient *s = (ShamirClient*) e;

    switch((intptr_t) vparam) {
        case H_MODULUS:
            {
                BIGNUM *bn = NULL;
                if (!BN_hex2bn(&bn, in_s.c_str()))
                    return errh->error("Failed to parse modulus parameter");
                if (s->_modulus)
                    BN_free(s->_modulus);
                s->_modulus = BN_dup(bn);
                BN_free(bn);
                break;
            }
        case H_THRESHOLD:
            {
                int threshold;
                if (!cp_integer(in_s, &threshold))
                        return errh->error("threshold parameter must be integer");
                s->_threshold = threshold;
                break;
            }
        case H_ACTIVE:
            {
                s->send_request();
            }
        default:
            {
 //               BRN_DEBUG("Invalid call to write handler");
            }
    }
    return 0;
}

void ShamirClient::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("modulus", read_param, H_MODULUS);
  add_write_handler("modulus", write_param, H_MODULUS);
  add_write_handler("threshold", write_param, H_THRESHOLD);
  add_write_handler("active", write_param, H_ACTIVE);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(ShamirClient)
ELEMENT_LIBS(-lssl -lcrypto)
