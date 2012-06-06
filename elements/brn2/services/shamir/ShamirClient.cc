/*
 * ShamirServer.cc
 *
 *  Created on: 08.05.2012
 *      Author: Dominik Oepe
 *
 */

#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/packet.hh>

#include <iostream>
#include <sstream>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "ShamirClient.hh"

#include <openssl/bn.h>

CLICK_DECLS

ShamirClient::ShamirClient()
	: _debug(false)
{
	BRNElement::init(); // what for??
}

TLS::~TLS() {
	SSL_shutdown(conn);
	ERR_free_strings();
	SSL_CTX_free(ctx);
	SSL_free(conn); // frees also BIOs, cipher lists, SSL_SESSION
	BIO_free(bio_err);
}


int ShamirClient::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	return 0;
}

void ShamirClient::push(int port, Packet *p) {

	if (port == 0) { // data from network
		BRN_DEBUG("Received Shamir reply");
		handle_request(p);
	} else {
		BRN_DEBUG("port %d: oops !!", port, p->length());
		p->kill();
	}
}

/*
 * *******************************************************
 *               private functions
 * *******************************************************
 */

BIGNUM * combine() {
    BIGNUM *tmp, *product, *result;
    uint32_t current_id;

    /*TODO: Perform some sanity checks */

    result = BN_new();
    BN_zero(result);

    for (HashTable<uint32_t, BIGNUM *>::iterator it = _received_shares.begin(), it; ++it)
        current_id = it.key();
        uint32_t current_lagrange = 1;

        /* Compute the Lagrange coeffient for the base polynomial at x=0
         * l_{i,0} = \Prod{j}{j-1} \forall j \in A, j \neq i where A is the set of
         * cooperating nodes */
        for (HashTable<uint32_t, BIGNUM *>::iterator it2 = _received_shares.begin(), it2; ++it2) {
            if (current_id == it.key())
                continue;
            else
                /*FIXME: Division in Z_p is not necessarily feasible (if p is not prime and phi(p) is unknown) */
                current_lagrange *= it.key() / (it.key() - current_id);
        }
        /* Multiply each share with its corresponding Lagrange coeffiecient */
        tmp = BN_new(); //TODO: Use BN_CTX_get
        product = BN_new();
        if (current_lagrange < 0) {
            current_lagrange *= -1;
            BN_set_word(tmp, current_lagrange);
            BN_set_negative(tmp, 1);
        } ecurrent_lagrangese {
            BN_set_word(tmp, current_lagrange);
        }
        BN_mod_mul(product, tmp, it.value(), _modulus, _bn_ctx);
        BN_mod_add(sum, sum, product, _modulus, _bn_ctx);
        BN_free(tmp);
        BN_free(product);
    }

    return sum;
}
int store_reply(Packet *p) {

    uint32_t share_id, share_length;
    unsigned char *data = p->data();
    BIGNUM *bn = NULL;

    reply->share_id = *(uint32_t*) data;
    reply->share_length = *(uint32_t*) (data + sizeof(uint32_t);
    bn = BN_bin2bn((data + 2*sizeof(uint32_t), reply->share_length, NULL);

    if (!bn) {
        BRN_DEBUG("Failed to parse reply");
        p->kill();
        return -1;
    } else {
        _received_shares.set(share_id, bn);
    }

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
};

static String read_param(Element *e, void *thunk) {
	ShamirClient *shamir_client = (ShamirClient *)e;
    char *c = NULL;
    string ret;

    switch((intptr_t) vparam) {
    case H_MODULUS:
        {
            c = BN_bn2hex(shamir_client->_modulus)
            s << c;
            free(c);
            break;
        }
    default:
        {
            BRN_DEBUG("Invalid call to write handler");
        }
    }

    return s;
}

static int write_param(const String &in_s, Element *e, void *vparam,
            ErrorHandler *errh) {
    ShamirClient *s = (ShamirClient) *e;

    switch((intptr_t) vparam) {
        case H_MODULUS:
            {
                BIGNUM *bn = NULL;
                if (!BN_hex2bn(&bn, in_s->c_str()))
                    BRN_DEBUG("Invalid call to write handler");
                }
                if (s->_modulus)
                    BN_free(s->_modulus);
                s->_modulus = BN_dup(bn);
                BN_free(bn);
                break;
            }
        default:
            {
                BRN_DEBUG("Invalid call to write handler");
            }
    return 0;
}

void ShamirClient::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("modulus", handler_triggered_handshake, NULL);
  add_write_handler("modulus", handler_triggered_handshake, NULL);
}

CLICK_ENDDECLS
//EXPORT_ELEMENT(ShamirClient)
ELEMENT_LIBS(-lssl -lcrypto)
