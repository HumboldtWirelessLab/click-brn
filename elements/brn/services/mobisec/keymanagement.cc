/*
 * KeyManagement.cc -- Key Managament
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

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "keymanagement.hh"

#define MIN_KEYLEN 5
#define MAX_KEYLEN 16

CLICK_DECLS

KeyManagement::KeyManagement()
	: _debug(false)
{
	BRNElement::init();
}

KeyManagement::~KeyManagement() {
	free(seed);
	keylist.clear();
}

int KeyManagement::initialization() {

	ctrl_data.cardinality = 0;
	ctrl_data.key_len = 0;
	ctrl_data.seed_len = 0;
	ctrl_data.timestamp = 0;

	seed = NULL;

	BACKBONE_AVAIL = false;

	return 0;
}

/*
 * *******************************************************
 *         		useful getter & setter functions
 * *******************************************************
 */
int32_t KeyManagement::get_validity_start_time(){
	return ctrl_data.timestamp;
}

void KeyManagement::set_validity_start_time(int32_t time) {
	ctrl_data.timestamp = time;
}

void KeyManagement::set_cardinality(int card) {
	ctrl_data.cardinality = card;
}

int KeyManagement::get_cardinality() {
	return ctrl_data.cardinality;
}

void KeyManagement::set_keylen(int len) {
	ctrl_data.key_len = (MIN_KEYLEN<=len && len<=MAX_KEYLEN)? len : 5;
}

void KeyManagement::set_seedlen(int len) {
	ctrl_data.seed_len = len;
}

int KeyManagement::get_keylen() {
	return ctrl_data.key_len;
}

void KeyManagement::set_key_timeout(int timeout) {
	key_timeout = timeout;
}

void KeyManagement::set_seed(const unsigned char *data) {
	if (!data) {
		click_chatter("seed is null");
		return;
	}

	if(ctrl_data.seed_len > 0) {
		seed = (unsigned char *) realloc(seed, ctrl_data.seed_len);
		memcpy(seed, data, ctrl_data.seed_len);
	} else {
		click_chatter("Trying to set seed having seed length 0.");
	}
}

data_t *KeyManagement::get_seed() {
	return seed;
}

bool KeyManagement::set_ctrl_data(crypto_ctrl_data *data) {
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
crypto_ctrl_data *KeyManagement::get_ctrl_data() {
	return &ctrl_data;
}

Vector<String> KeyManagement::get_keylist() {
	return keylist;
}

data_t *KeyManagement::get_keylist_string() {
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
void KeyManagement::gen_seeded_keylist() {
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
void KeyManagement::install_keylist_cli_driv(data_t *_seed) {
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
void KeyManagement::gen_keylist() {
	unsigned char *ith_key = (unsigned char *)malloc(ctrl_data.key_len);

	RAND_seed("Wer nichts als Informatik versteht, versteht auch die nicht recht -- L1cht3nb3r9", 80);

	click_chatter("gen_keylist card:%i", ctrl_data.cardinality);
	for(int i=0; i<ctrl_data.cardinality && ctrl_data.cardinality <= 1000; i++) {
		// Generate a new key for the list
		RAND_bytes(ith_key, ctrl_data.key_len);

		String s((const char*)ith_key, ctrl_data.key_len);

		// click_chatter("KEYSERVER GENERATE %dth key: %s",i, s.quoted_hex().c_str());

		// Build the keylist of type vector
		keylist.push_back(s);
	}

	free(ith_key);
}

/* for backbone node and keyserver */
void KeyManagement::install_keylist_srv_driv(data_t *_keylist) {
	keylist.clear();

	for(int i=0; i<ctrl_data.cardinality; i++) {
		//for(int j=0; j<ctrl_data.key_len; j++) {
			int index = i*ctrl_data.key_len;
			String ith_key((const char*)(&(_keylist[index])), ctrl_data.key_len);
			keylist.push_back(ith_key);
		//}
	}
}

/*
 * *******************************************************
 *         functions for both protocols
 * *******************************************************
 */

/* for backbone node and keyserver */
void KeyManagement::install_keylist(Vector<String> _keylist) {
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
bool KeyManagement::install_key_on_phy(Element *_wepencap, Element *_wepdecap) {
	int32_t time_now = Timestamp::now().msecval();
	int32_t time_keylist = ctrl_data.timestamp;

	// Yet another reasonability check
	if (ctrl_data.timestamp == 0 || time_now >= time_keylist + ctrl_data.cardinality*key_timeout) {
		click_chatter("INFO: crypto material not existent or expired");

		if (BACKBONE_AVAIL) {
				BACKBONE_AVAIL = false;
				click_chatter("%d BACKBONE:0", time_now);
		}

		return false;
	}

	if (!BACKBONE_AVAIL) {
			BACKBONE_AVAIL = true;
			click_chatter("%d BACKBONE:1", time_now);
	}

	// Note: The implicit int-type handling causes automatically a round down
	int index = (time_now - time_keylist)/key_timeout;

	if (!(0 <= index && index < keylist.size())) {
		click_chatter("ERROR: No keys available for index %i", index);
		return false;
	}

	const String key = keylist[index];

	const String handler = "key";

	/* **********************************
	 * Set keys in WEPencap and WEPdecap
	 * ***********************************/
	int success1;
	success1 = HandlerCall::call_write(_wepencap, handler, key, NULL);
	if(success1==0) click_chatter("WEPencap: new key(%d): %s", index, key.quoted_hex().c_str());
	else click_chatter("WEPencap: ERROR while setting new key (%s)", key.quoted_hex().c_str());

	int success2;
	success2 = HandlerCall::call_write(_wepdecap, handler, key, NULL);
	if(success2==0) click_chatter("WEPdecap: new key(%d): %s", index,  key.quoted_hex().c_str());
	else click_chatter("WEPdecap: ERROR while setting new key (%s)", key.quoted_hex().c_str());

	return (success1==0 && success2==0)? true : false;
}



CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
ELEMENT_PROVIDES(KeyManagement)
