/*
 * tls.hh
 *
 *  Created on: 18.04.2012
 *      Author: aureliano
 */

#ifndef TLS_HH_
#define TLS_HH_

#include <string>
#include <queue>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashmap.hh>
#include <click/packet.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"

#include <openssl/ssl.h>
#include <openssl/err.h>


using namespace std;

const size_t BUFSIZE = 128;
typedef unsigned char data_t;
enum role_t {SERVER, CLIENT};

CLICK_DECLS



class com_obj {
public:
	com_obj(SSL_CTX *ctx, enum role_t role) {
		conn = SSL_new(ctx);
		if (!conn) {print_err(); throw std::bad_alloc();}

		bioIn = BIO_new(BIO_s_mem());
		if (!bioIn) {print_err(); throw std::bad_alloc();}

		bioOut = BIO_new(BIO_s_mem());
		if (!bioOut) {print_err(); throw std::bad_alloc();}

		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
		if (!bio_err) {print_err(); throw std::bad_alloc();}
		SSL_set_bio(conn,bioIn,bioOut); // connect the ssl-object to the bios

		// to read additional protocol bytes wenn calling SSL_pending containing more SSL records
		SSL_set_read_ahead(conn, 1);

		// Must be called before first SSL_read or SSL_write
		(role==CLIENT)? SSL_set_connect_state(conn) : SSL_set_accept_state(conn);

		wep_state = false;
	}
	~com_obj() {
		SSL_shutdown(conn);
		SSL_free(conn); // frees also BIOs, cipher lists, SSL_SESSION
		BIO_free(bio_err);
	}

	void print_err() {
		click_chatter("%s" , ERR_error_string(ERR_get_error(), NULL));
	}

	SSL* conn;
	BIO* bioIn;
	BIO* bioOut;
	BIO* bio_err;
	bool wep_state;
	EtherAddress sender_addr;

	queue<Packet *> pkt_storage;
};


class TLS : public BRNElement {
public:
	TLS();
	~TLS();

	const char *class_name() const { return "TLS"; }
	const char *port_count() const { return "2/2"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	void restart_tls();
	static void restart_trigger(Timer *t, void *element) { ((TLS *)element)->restart_tls(); }

	void add_handlers();

private:
	int _debug;
	EtherAddress _me;
	EtherAddress _ks_addr;

	String _role;
	enum role_t role;
	String keydir;

	SSL_CTX *ctx;

	com_obj *curr;

	// Server variable: Store the tls-conn for every communicating node
	HashMap<EtherAddress, com_obj*> com_table;

	void encrypt(Packet *p); // send application data
	void decrypt(); // receive application data

	bool do_handshake();

	void store_data(Packet *p);
	void clear_pkt_storage();
	void snd_stored_data();

	int snd_data();
	void rcv_data(Packet *p);

	void print_err();

	Timer restart_timer;
};

// For now, this functions is not integrable because
// SSL_CTX_set_default_passwd_cb needs a function pointer. But
// class functions do not provide static function pointers.
int pem_passwd_cb(char *buf, int size, int rwflag, void *password);



CLICK_ENDDECLS
#endif /* TLS_HH_ */
