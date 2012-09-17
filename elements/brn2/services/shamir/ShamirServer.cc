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

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "ShamirServer.hh"
#include "Shamir.hh"

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
        "ETHERADDRESS", cpkP, cpEthernetAddress, &_me,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	return 0;
}

int ShamirServer::initialize(ErrorHandler *) {
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

int ShamirServer::handle_request(Packet *p_in) {
    EtherAddress src_addr = BRNPacketAnno::src_ether_anno(p_in);
    BRN_DEBUG("Received request from %s", src_addr.unparse().c_str());

    struct shamir_reply reply;
    reply.share_id = _share_id;
    reply.share_len = BN_num_bytes(_share);

    if (reply.share_len > MAX_SHARESIZE) {
        BRN_DEBUG("Share is bigger than MAX_SHARESIZE");
        p_in->kill();
        return -1;
    }

    if (!BN_bn2bin(_share, reply.share)) {
        BRN_DEBUG("Failed to handle request");
        p_in->kill();
        return -1;
    }

    WritablePacket *p = p_in->push(sizeof(shamir_reply));
    BRNPacketAnno::set_ether_anno(p, _me, src_addr, ETHERTYPE_BRN);

    memcpy(p->data(), &reply, sizeof(struct shamir_reply));
    output(0).push(p);
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

    switch((intptr_t) thunk) {
    case H_MODULUS:
        {
            return String(BN_bn2hex(shamir_server->_modulus));
            break;
        }
    case H_SHARE:
        {
            return String(BN_bn2hex(shamir_server->_share));
            break;
        }
    case H_SHARE_ID:
        {
            return String(shamir_server->_share_id);
            break;
        }
    default:
        {
            return String();
        }
    }
}

static int write_param(const String &in_s, Element *e, void *vparam,
            ErrorHandler *errh) {
    ShamirServer *shamir_server = (ShamirServer*) e;
    String s = cp_uncomment(in_s);

    switch((intptr_t) vparam) {
        case H_MODULUS:
            {
                BIGNUM *bn = NULL;
                if (!BN_hex2bn(&bn, in_s.c_str()))
                    break;
//                    BRN_DEBUG("Invalid call to write handler");
                if (shamir_server->_modulus)
                    BN_free(shamir_server->_modulus);
                shamir_server->_modulus = BN_dup(bn);
                BN_free(bn);
                break;
            }
        case H_SHARE:
            {
                BIGNUM *bn = NULL;
                if (!BN_hex2bn(&bn, in_s.c_str()))
                    break;
//                    BRN_DEBUG("Invalid call to write handler");
                if (shamir_server->_share)
                    BN_free(shamir_server->_share);
                shamir_server->_share = BN_dup(bn);
                BN_free(bn);
                break;
            }
        case H_SHARE_ID:
            {
                int id;
                if (!cp_integer(s, &id))
                    return errh->error("id must be integer");
                shamir_server->_share_id = id;
                break;
            }
        default:
            {
//                BRN_DEBUG("Invalid call to write handler");
            }
    }
    return 0;
}

void ShamirServer::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("modulus", read_param, H_MODULUS);
  add_write_handler("modulus", write_param, H_MODULUS);
  add_read_handler("share", read_param, H_SHARE);
  add_write_handler("share", write_param, H_SHARE);
  add_read_handler("share_id", read_param, H_SHARE_ID);
  add_write_handler("share_id", write_param, H_SHARE_ID);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(ShamirServer)
ELEMENT_LIBS(-lssl -lcrypto)
