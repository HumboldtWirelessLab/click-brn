/*
 * keymanagement.cc -- Key Managament
 *
 *  Created on: 06.05.2012
 *      Author: aureliano
 */


#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include <openssl/err.h>
#include <openssl/rand.h>

#include "keymanagement.hh"

CLICK_DECLS

keymanagement::keymanagement()
	: _debug(false),
	  ci()
{
	BRNElement::init();
}

keymanagement::~keymanagement() {
}

crypto_info *keymanagement::get_crypto_info() {
	return &ci;
}

void keymanagement::generate_crypto_cli_driv() {

	ci.timestamp = time(NULL);
	ci.cardinality = 4;
	ci.key_len = 0;
	ci.seed_len = 20; //160 bit for sha1  make less than 20 byte

	RAND_seed("Some seed here =) 10f3jdsdfgj34409jg", 20);

	unsigned char *seed = new unsigned char[ci.seed_len];
	RAND_bytes(seed, ci.seed_len);

	ci.data = seed;
}


void keymanagement::generate_crypto_srv_driv() {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(keymanagement)
ELEMENT_LIBS(-lssl -lcrypto)
