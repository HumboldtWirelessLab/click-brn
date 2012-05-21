/*
 * keymanagement.hh -- Key Managament
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef KEYMANAGEMENT_HH_
#define KEYMANAGEMENT_HH_

#include <vector>

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
	int initialization();

	void set_cardinality(int card);

	void set_seed(const unsigned char *);
	unsigned char *get_seed();

	void set_ctrl_data(crypto_ctrl_data *);
	crypto_ctrl_data *get_ctrl_data();

	// The crypto_generator will be executed periodically by the key server
		/* CLIENT_DRIVEN */
	void gen_crypto_cli_driv();
	void constr_keylist_cli_driv();
		/* SERVER_DRIVEN */
	void gen_crypto_srv_driv();
	void constr_keylist_srv_driv();


	// This method uses the list to set the adequate key
	void install_key_on_phy();
	std::vector<String> keylist;
private:
	int _debug;

	crypto_ctrl_data ctrl_data;
	unsigned char *seed;



	void store_crypto_info();
};

CLICK_ENDDECLS
#endif /* KEYMANAGEMENT_HH_ */
