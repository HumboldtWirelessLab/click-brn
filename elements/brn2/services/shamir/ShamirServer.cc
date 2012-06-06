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

#include "ShamirServer.hh"

#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

CLICK_DECLS

ShamirServer::ShamirServer()
	: _debug(false)
{
	BRNElement::init();

    _bn_ctx = BN_CTX_new();
    if (_bn_ctx)
        BN_CTX_init(_bn_ctx);
    else
        BRN_DEBUG("Failed to initialize ShamirServer");
}

ShamirServer::~ShamirServer() {
    if (_bn_ctx)
        BN_CTX_free(_bn_ctx);
    if (_modulus)
        BN_clear_free(_modulus);
    if (_share)
        BN_clear_free(_share);
}


int ShamirServer::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	return 0;
}

void ShamirServer::push(int port, Packet *p) {

	if (port == 0) { // data from network
		BRN_DEBUG("Received Shamir request");
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

int ShamirServer::handle_request(Packet *p) {

    struct shamir_reply reply;
    reply.share_id = _share_id;
    reply.share_len = BN_bn2bin(_share, reply.share);
    if (!reply.share_len) {
        BRN_DEBUG("Failed to handle request");
        p->kill();
        return -1;
    }

    Packet *reply_packet = Packet::make(128, NULL, 2*sizeof(uint32_t)+reply->share_length);
    return 0;
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */

enum {
    H_MODULUS,
    H_SHARE,
    H_SHARE_ID
};

static String read_param(Element *e, void *thunk) {
	ShamirServer *shamir_server = (ShamirServer *)e;
    char *c = NULL;
    ostringstream s;

    switch((intptr_t) thunk) {
    case H_MODULUS:
        {
            c = BN_bn2hex(shamir_server->_modulus);
            s << c;
            free(c);
            break;
        }
    case H_SHARE:
        {
            c = BN_bn2hex(shamir_server->_share);
            s << c;
            free(c);
            break;
        }
    case H_SHARE_ID:
        {
            s << shamir_server->_share_id;
            break;
        }
    default:
        {
            BRN_DEBUG("Invalid call to write handler");
        }
    }

    return s.str();
}

static int write_param(const String &in_s, Element *e, void *vparam,
            ErrorHandler *errh) {
    ShamirServer *s = (ShamirServer*) e;

    switch((intptr_t) vparam) {
        case H_MODULUS:
            {
                BIGNUM *bn = NULL;
                if (!BN_hex2bn(&bn, in_s.c_str()))
                    BRN_DEBUG("Invalid call to write handler");
                if (s->_modulus)
                    BN_free(s->_modulus);
                s->_modulus = BN_dup(bn);
                BN_free(bn);
                break;
            }
        case H_SHARE:
            {
                BIGNUM *bn = NULL;
                if (!BN_hex2bn(&bn, in_s->c_str()))
                    BRN_DEBUG("Invalid call to write handler");
                if (s->_share)
                    BN_free(s->_share);
                s->_share = BN_dup(bn);
                BN_free(bn);
                break;
            }
        case H_SHARE_ID:
            {
                unsigned int id;
                stringstream ss(in_s);

                if ((ss >> id).fail()) {
                    BRN_DEBUG("Invalid call to write handler");
                    break;
                }
                s->_share_id = id;
                break;
            }
        default:
            {
                BRN_DEBUG("Invalid call to write handler");
            }
    }
    return 0;
}

void ShamirServer::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("modulus", handler_triggered_handshake, H_MODULUS);
  add_write_handler("modulus", handler_triggered_handshake, H_MODULUS);
  add_read_handler("share", handler_triggered_handshake, H_SHARE);
  add_write_handler("share", handler_triggered_handshake, H_SHARE);
  add_read_handler("share_id", handler_triggered_handshake, H_SHARE_ID);
  add_write_handler("share_id", handler_triggered_handshake, H_SHARE_ID);
}

CLICK_ENDDECLS
//EXPORT_ELEMENT(ShamirServer)
ELEMENT_LIBS(-lssl -lcrypto)
