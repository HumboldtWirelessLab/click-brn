/*
 * keymanagement.hh -- Key Managament
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef KEYMANAGEMENT_HH_
#define KEYMANAGEMENT_HH_

#include <click/timestamp.hh>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/element.hh>
#include <click/confparse.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

struct crypto_ctrl_data {
	int32_t timestamp;
	int cardinality; 	// also interpretable as rounds
	int key_len; 		// WEP key length
	int seed_len;
};


class keymanagement : public BRNElement {
public:
	keymanagement();
	~keymanagement();

	const char *class_name() const { return "keymanagement"; }
	int initialization();

	void set_validity_start_time(int32_t time);
	int32_t get_validity_start_time();

	void set_cardinality(int card);
	void set_key_timeout(int timeout);

	void set_seed(const unsigned char *);
	unsigned char *get_seed();

	// Useful for installation of complete ctrl_data
	void set_ctrl_data(crypto_ctrl_data *);
	crypto_ctrl_data *get_ctrl_data();

	// The crypto_generator will be executed periodically by the key server
		/* CLIENT_DRIVEN */
	void gen_crypto_cli_driv();
	void constr_keylist_cli_driv();
		/* SERVER_DRIVEN */
	void gen_crypto_srv_driv();
	void constr_keylist_srv_driv();


	// This method uses the list to set the adequate key on phy layer
	void install_key_on_phy(Element *_wepencap, Element *_wepdecap);
private:
	int _debug;

	crypto_ctrl_data ctrl_data;
	unsigned char *seed;

	// key_timeout is needed for the calculation of the keylist's index
	int key_timeout;


	Vector<String> keylist;

	void store_crypto_info();
};

CLICK_ENDDECLS
#endif /* KEYMANAGEMENT_HH_ */
