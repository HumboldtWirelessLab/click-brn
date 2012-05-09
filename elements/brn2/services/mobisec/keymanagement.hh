/*
 * keymanagement.hh -- Key Managament
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef keymanagement_HH_
#define keymanagement_HH_


#include <string>
#include <click/element.hh>
#include <click/confparse.hh>
#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class crypto_info {
public:
	time_t timestamp;
	int cardinality; // also interpretable as rounds
	int key_len;
	int seed_len;
	void *data; // todo: free data after destruction
};

class keymanagement : public BRNElement {
public:
	keymanagement();
	~keymanagement();

	const char *class_name() const { return "keymanagement"; }

	// The crypto_generator will be executed periodically by the key server
	void generate_crypto_cli_driv();
	void generate_crypto_srv_driv();

	// This function will be called by both server and client to install keys
	void install_keys_on_phy();

	crypto_info *get_crypto_info();

private:
	int _debug;
	crypto_info ci;

	void store_crypto_info();
};

CLICK_ENDDECLS
#endif /* keymanagement_HH_ */
