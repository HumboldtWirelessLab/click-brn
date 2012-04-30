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
#include "elements/brn2/brnelement.hh"

#include <openssl/ssl.h>


using namespace std;

const size_t BUFSIZE = 128;
typedef unsigned char data_t;

CLICK_DECLS



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

	void encrypt(Packet *p); // send application data
	void receive(); // receive application data
	int do_handshake();

	void add_handlers();
private:
	String role;
	String keydir;

	SSL_CTX *ctx;
	SSL* conn;
	BIO* bioIn;
	BIO* bioOut;
	BIO* bio_err;

	int _debug;
	EtherAddress _me;

	// todo: meherer Verbindungen verwalten über hashmap
	//Hashmap<EtherAddress, SSL_CTX> ssl_table;
	EtherAddress dst;

	queue<Packet *> pkt_storage;
	void store_data(Packet *p);
	void snd_stored_data();

	int snd_data();
	int rcv_data(Packet *p);
	void print_err();
};

// For now, this functions is not integrable because
// SSL_CTX_set_default_passwd_cb needs a function pointer. But
// class functions do not provide static function pointers.
int pem_passwd_cb(char *buf, int size, int rwflag, void *password);

CLICK_ENDDECLS
#endif /* TLS_HH_ */
