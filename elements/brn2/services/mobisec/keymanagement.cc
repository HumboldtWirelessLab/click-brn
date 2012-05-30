/*
 * keymanagement.cc -- Key Managament
 *
 *  Created on: 06.05.2012
 *      Author: aureliano
 *
 * The key management holds the data structure for the cryptographic control data.
 * Furthermore it stores the crypto material in order to apply it on the WEP-Module.
 *
 * Todo: Need better specification for key_len, seed_len, cardinality.
 */

#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/handlercall.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "keymanagement.hh"

CLICK_DECLS

keymanagement::keymanagement()
	: _debug(false)
{
	BRNElement::init();
}

keymanagement::~keymanagement() {
	free(seed);
	keylist.clear();
}

int keymanagement::initialization() {
	crypto_ctrl_data ctrl_data = {0 , 0, 0, 0};
	seed = NULL;

	return 0;
}

/*
 * *******************************************************
 *         		useful getter & setter functions
 * *******************************************************
 */

void keymanagement::set_cardinality(int card) {
	ctrl_data.cardinality = card;
}

void keymanagement::set_seed(const unsigned char *data) {
	if(ctrl_data.seed_len > 0) {
		seed = (unsigned char *) realloc(seed, ctrl_data.seed_len);
		memcpy(seed, data, ctrl_data.seed_len);
	} else {
		BRN_ERROR("Trying to set seed having seed length 0.");
	}
}

unsigned char *keymanagement::get_seed() {
	return seed;
}

void keymanagement::set_ctrl_data(crypto_ctrl_data *data) {
	memcpy(&ctrl_data, data, sizeof(crypto_ctrl_data));
}
crypto_ctrl_data *keymanagement::get_ctrl_data() {
	return &ctrl_data;
}

/*
 * *******************************************************
 *         functions for CLIENT_DRIVEN protocol
 * *******************************************************
 */

void keymanagement::gen_crypto_cli_driv() {
	ctrl_data.timestamp = time(NULL);
	ctrl_data.cardinality = 4;
	ctrl_data.key_len = 5;
	ctrl_data.seed_len = 20; //160 bit for sha1  make less than 20 byte

	RAND_seed("10f3jxYEAH--.dsdfgj34409jg", 20);

	// Deallocation and allocation to prepare for dynamic seeding.
	seed = (unsigned char *) realloc(seed, ctrl_data.seed_len);

	if (seed != NULL)
		RAND_bytes(seed, ctrl_data.seed_len);
	else
		BRN_ERROR("Seed generation failed.");
}

void keymanagement::constr_keylist_cli_driv() {
	unsigned char *curr_key = seed;

	for(int i=0; i < ctrl_data.cardinality; i++) {
		curr_key = SHA1((const unsigned char *)curr_key, ctrl_data.seed_len, NULL);

		String s;
		s.append((const char*)curr_key, ctrl_data.key_len);

		// todo: not really tested
		keylist.push_back(s);
	}
}


/*
 * *******************************************************
 *         functions for SERVER_DRIVEN protocol
 * *******************************************************
 */

void keymanagement::gen_crypto_srv_driv() {

}

/*
 * *******************************************************
 *       					other
 * *******************************************************
 */

// This method uses the list to set the adequate key
void keymanagement::install_key_on_phy(Element *_wepencap, Element *_wepdecap) {
	// todo: select right key from list depending on time
	const String handler = "key";
	const String key = "weizenbaum";
	int success = HandlerCall::call_write(_wepencap, handler, key, NULL);
	BRN_DEBUG("new key: %s", HandlerCall::call_read(_wepencap, handler).c_str() );
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(keymanagement)
