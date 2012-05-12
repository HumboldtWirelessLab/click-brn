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
	: _debug(false)
{
	BRNElement::init();

	ctrl_data = (crypto_ctrl_data *)malloc(sizeof(crypto_ctrl_data));
	if (ctrl_data == NULL) BRN_ERROR("Could not allocate crypto_ctrl_data.");
}

keymanagement::~keymanagement() {
	free(ctrl_data);
	free(seed);
	//todo: free(keylist)
}

void keymanagement::set_cardinality(int card) {
	ctrl_data->cardinality = card;
}

void keymanagement::set_seed(const unsigned char *data) {
	if(ctrl_data->seed_len > 0) {
		if(seed == NULL) seed = new unsigned char[ctrl_data->seed_len];
		memcpy(seed, data, ctrl_data->seed_len);
	} else {
		BRN_ERROR("Trying to set seed having seed length 0.");
	}
}

unsigned char *keymanagement::get_seed() {
	return seed;
}

void keymanagement::set_ctrl_data(crypto_ctrl_data *data) {
	memcpy(ctrl_data, data, sizeof(crypto_ctrl_data));
}
crypto_ctrl_data *keymanagement::get_ctrl_data() {
	return ctrl_data;
}

void keymanagement::generate_crypto_cli_driv() {

	ctrl_data->timestamp = time(NULL);
	ctrl_data->cardinality = 4;
	ctrl_data->key_len = 0;
	ctrl_data->seed_len = 20; //160 bit for sha1  make less than 20 byte

	RAND_seed("10f3jxYEAH--->dsdfgj34409jg", 20);

	if(seed == NULL) seed = new unsigned char[ctrl_data->seed_len];
	RAND_bytes(seed, ctrl_data->seed_len);
}


void keymanagement::generate_crypto_srv_driv() {

}


void keymanagement::install_keys_on_phy() {

}

CLICK_ENDDECLS
EXPORT_ELEMENT(keymanagement)
ELEMENT_LIBS(-lssl -lcrypto)
