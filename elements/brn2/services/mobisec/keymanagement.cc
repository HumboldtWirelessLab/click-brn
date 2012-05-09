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

#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

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
	ci.seed_len = sizeof(int); //150 bit for sha1  make less than 32 byte

	ci.data = new int(1337); // use char array
}


void keymanagement::generate_crypto_srv_driv() {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(keymanagement)
ELEMENT_LIBS(-lssl)
