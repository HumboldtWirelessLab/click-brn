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
#include <click/timestamp.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "keymanagement.hh"

#define MIN_KEYLEN 5
#define MAX_KEYLEN 16

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
	//crypto_ctrl_data ctrl_data = {0 , 0, 0, 0};

	seed = NULL;

	return 0;
}

/*
 * *******************************************************
 *         		useful getter & setter functions
 * *******************************************************
 */
int32_t keymanagement::get_validity_start_time(){
	return ctrl_data.timestamp;
}

void keymanagement::set_validity_start_time(int32_t time) {
	ctrl_data.timestamp = time;
}

void keymanagement::set_cardinality(int card) {
	ctrl_data.cardinality = card;
}

int keymanagement::get_cardinality() {
	return ctrl_data.cardinality;
}

void keymanagement::set_keylen(int len) {
	ctrl_data.key_len = (MIN_KEYLEN<=len && len<=MAX_KEYLEN)? len : 5;
}

void keymanagement::set_seedlen(int len) {
	ctrl_data.seed_len = len;
}

int keymanagement::get_keylen() {
	return ctrl_data.key_len;
}

void keymanagement::set_key_timeout(int timeout) {
	key_timeout = timeout;
}

void keymanagement::set_seed(const unsigned char *data) {
	if(ctrl_data.seed_len > 0) {
		seed = (unsigned char *) realloc(seed, ctrl_data.seed_len);
		memcpy(seed, data, ctrl_data.seed_len);
	} else {
		click_chatter("Trying to set seed having seed length 0.");
	}
}

data_t *keymanagement::get_seed() {
	return seed;
}

bool keymanagement::set_ctrl_data(crypto_ctrl_data *data) {
	// Plausibility check
	if (data &&
		data->cardinality > 0 &&
		data->key_len > 0 &&
		data->timestamp > 0) {

		memcpy(&ctrl_data, data, sizeof(crypto_ctrl_data));
		return true;
	} else {
		click_chatter("Plausibility check failed. Wrong ctrl_data, nothing installed!");
		return false;
	}
}
crypto_ctrl_data *keymanagement::get_ctrl_data() {
	return &ctrl_data;
}

Vector<String> keymanagement::get_keylist() {
	return keylist;
}

data_t *keymanagement::get_keylist_string() {
	data_t *keylist_string = (unsigned char*)malloc(ctrl_data.cardinality * ctrl_data.key_len);
	if(!keylist_string) { click_chatter("NO MEM FOR keylist_string");}

	for(int i=0; i<ctrl_data.cardinality; i++) {
		// Build a keylist of type char
		memcpy(keylist_string+(i*ctrl_data.key_len), keylist[i].data(), ctrl_data.key_len);
	}

	return keylist_string;
}

/*
 * *******************************************************
 *         functions for CLIENT_DRIVEN protocol
 * *******************************************************
 */

/* This function should only used by the keyserver */
void keymanagement::gen_seeded_keylist() {
	// Todo: check if all information are available for generation

	RAND_seed("Wir möchten gerne, dass der Computer das tut, was wir wollen; doch er tut nur das, was wir schreiben. Ich weiß auch nicht warum -- W.", 80);

	// Deallocation and allocation to prepare for dynamic seeding.
	seed = (unsigned char *) realloc(seed, ctrl_data.seed_len);

	if (seed != NULL) {
		RAND_bytes(seed, ctrl_data.seed_len);
	} else {
		click_chatter("Seed generation failed.");
	}

	install_keylist_cli_driv(seed);
}

/* for backbone node and keyserver */
void keymanagement::install_keylist_cli_driv(data_t *_seed) {
	keylist.clear();

	data_t *curr_key = (data_t *)_seed;

	for(int i=0; i < ctrl_data.cardinality; i++) {
		curr_key = SHA1((const unsigned char *)curr_key, ctrl_data.seed_len, NULL);

		// We use only key_len-bytes of the the hash value
		String s((const char*)curr_key, ctrl_data.key_len);

		keylist.push_back(s);
	}
}


/*
 * *******************************************************
 *         functions for SERVER_DRIVEN protocol
 * *******************************************************
 */

/* This function should only used by the keyserver */
void keymanagement::gen_keylist() {
	unsigned char *ith_key = (unsigned char *)malloc(ctrl_data.key_len);

	RAND_seed("Wer nichts als Informatik versteht, versteht auch die nicht recht -- L1cht3nb3r9", 80);

	click_chatter("gen_keylist card:%i", ctrl_data.cardinality);
	for(int i=0; i<ctrl_data.cardinality && ctrl_data.cardinality <= 1000; i++) {
		// Generate a new key for the list
		RAND_bytes(ith_key, ctrl_data.key_len);

		String s((const char*)ith_key, ctrl_data.key_len);

		// Build the keylist of type vector
		keylist.push_back(s);
	}

	free(ith_key);
}

/* for backbone node and keyserver */
void keymanagement::install_keylist_srv_driv(data_t *_keylist) {
	keylist.clear();

	for(int i=0; i<ctrl_data.cardinality; i++) {
		for(int j=0; j<ctrl_data.key_len; j++) {
			int index = i*ctrl_data.key_len + j;
			String ith_key((const char*)(&(_keylist[index])), ctrl_data.key_len);
			keylist.push_back(ith_key);
		}
	}
}

/*
 * *******************************************************
 *         functions for both protocols
 * *******************************************************
 */

/* for backbone node and keyserver */
void keymanagement::install_keylist(Vector<String> _keylist) {
	keylist.clear();

	for(int i=0; i<_keylist.size();i++) {
		keylist.push_back(_keylist[i]);
	}
}

/*
 * *******************************************************
 *       					other
 * *******************************************************
 */

// This method uses the list to set the adequate key
void keymanagement::install_key_on_phy(Element *_wepencap, Element *_wepdecap) {
	int32_t time_now = Timestamp::now().msecval();
	int32_t time_keylist = ctrl_data.timestamp;

	// Yet another reasonability check
	if (time_now - time_keylist > ctrl_data.cardinality*key_timeout) {
		click_chatter("INFO: crypto material not existent or expired");
		return;
	}

	// Note: The implicit int-type handling causes automatically a round down
	int index = (time_now - time_keylist)/key_timeout;

	if (!(0 <= index && index < keylist.size())) {
		click_chatter("ERROR: No keys available for index %i", index);
		return;
	}

	const String key = keylist[index];

	const String handler = "key";

	/* **********************************
	 * Set keys in WEPencap and WEPdecap
	 * ***********************************/
	int success;
	success = HandlerCall::call_write(_wepencap, handler, key, NULL);
	if(success==0) click_chatter("WEPencap: new key(%d): %s", index, HandlerCall::call_read(_wepencap, handler).c_str() );
	else click_chatter("WEPencap: ERROR while setting new key");

	success = HandlerCall::call_write(_wepdecap, handler, key, NULL);
	if(success==0) click_chatter("WEPdecap: new key(%d): %s", index, HandlerCall::call_read(_wepdecap, handler).c_str() );
	else click_chatter("WEPdecap: ERROR while setting new key");
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(keymanagement)
