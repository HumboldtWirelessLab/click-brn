/*
 * keymanagement.hh -- Key Managament
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef KEYMANAGEMENT_HH_
#define KEYMANAGEMENT_HH_


#include <string>
#include <click/element.hh>
#include <click/confparse.hh>
#include "elements/brn2/brnelement.hh"

CLICK_DECLS

struct crypto_ctrl_data {
	time_t timestamp;
	int cardinality; // also interpretable as rounds
	int key_len;
	int seed_len;
};

class keymanagement : public BRNElement {
public:
	keymanagement();
	~keymanagement();

	const char *class_name() const { return "keymanagement"; }

	void set_cardinality(int card);

	void set_seed(const unsigned char *);
	unsigned char *get_seed();

	void set_ctrl_data(crypto_ctrl_data *);
	crypto_ctrl_data *get_ctrl_data();

	// The crypto_generator will be executed periodically by the key server
	void generate_crypto_cli_driv();
	void generate_crypto_srv_driv();

	// This function will be called by both server and client to install keys
	void install_keys_on_phy();

private:
	int _debug;

	crypto_ctrl_data *ctrl_data;
	unsigned char *seed;

	//todo: store structure for key list

	void store_crypto_info();
};

CLICK_ENDDECLS
#endif /* KEYMANAGEMENT_HH_ */
