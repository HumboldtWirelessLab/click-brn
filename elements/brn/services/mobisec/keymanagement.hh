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

typedef unsigned char data_t;

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
	int get_cardinality();

	void set_keylen(int);
	int get_keylen();

	void set_key_timeout(int timeout);

	void set_seed(const data_t *);
	data_t *get_seed();
	void set_seedlen(int);

	// Useful for installation of complete ctrl_data
	bool set_ctrl_data(crypto_ctrl_data *);
	crypto_ctrl_data *get_ctrl_data();

	Vector<String> get_keylist();
	data_t* get_keylist_string(); // Same as get_keylist, but returning a string of chars

	// The crypto_generator will be executed periodically by the key server
		/* CLIENT_DRIVEN */
	void gen_seeded_keylist();
	void install_keylist_cli_driv(data_t *payload_seed);
		/* SERVER_DRIVEN */
	void gen_keylist();
	void gen_keylist_string();
	void install_keylist_srv_driv(data_t *payload_keylist);

	void install_keylist(Vector<String> keylist);

	// This method uses the list to set the adequate key on phy layer
	bool install_key_on_phy(Element *_wepencap, Element *_wepdecap);
private:
	int _debug;

	crypto_ctrl_data ctrl_data;
	data_t *seed;
	Vector<String> keylist;

	int key_timeout; // session time


	void store_crypto_info();
};


CLICK_ENDDECLS
#endif /* KEYMANAGEMENT_HH_ */
