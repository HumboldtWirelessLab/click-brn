/*
 * tls.hh
 *
 *  Created on: 18.04.2012
 *      Author: kuehne@informatik.hu-berlin.de
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

#include "elements/brn/brnelement.hh"

#include <openssl/ssl.h>
#include <openssl/err.h>


using namespace std;

const size_t BUFSIZE = 128;
typedef unsigned char data_t;
enum role_t {SERVER, CLIENT};

CLICK_DECLS



class com_obj {
public:
	Vector <SSL*> old_connections;

	com_obj(SSL_CTX *_ctx, enum role_t _role) {
		role = _role;
		ctx = _ctx;
		traffic_cnt = 0;
               conn = NULL;

		conn = SSL_new(ctx);
		if (!conn) {throw std::bad_alloc();}

		bioIn = BIO_new(BIO_s_mem());
		if (!bioIn) {throw std::bad_alloc();}

		bioOut = BIO_new(BIO_s_mem());
		if (!bioOut) {throw std::bad_alloc();}

		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
		if (!bio_err) {throw std::bad_alloc();}

		SSL_set_bio(conn,bioIn,bioOut); // connect the ssl-object to the bios

		// to read additional protocol bytes when calling SSL_pending containing more SSL records
		SSL_set_read_ahead(conn, 1);

		// Must be called before first SSL_read or SSL_write
		(role==CLIENT)? SSL_set_connect_state(conn) : SSL_set_accept_state(conn);
		
		old_connections.clear();
	}

	~com_obj() {
		SSL_free(conn); // frees also BIOs, cipher lists, SSL_SESSION
               conn = NULL;
		BIO_free(bio_err);

		while (!pkt_storage.empty()) {
			pkt_storage.pop();
		}
	}

	void refresh() {
        /*if ( conn != NULL ) {
            SSL_set_bio(conn,NULL,NULL);
            SSL_free(conn);
        }*/
		old_connections.push_back(conn);
		conn = SSL_new(ctx);
		SSL_set_bio(conn,bioIn,bioOut);
		SSL_set_read_ahead(conn, 1);
		(role==CLIENT)? SSL_set_connect_state(conn) : SSL_set_accept_state(conn);
	}

	enum role_t role;

	SSL_CTX* ctx;
	SSL* conn;						// This is actually the important ssl connection object
	BIO* bioIn;
	BIO* bioOut;
	BIO* bio_err;
	EtherAddress dst_addr;			// This will be the other destination address of the p2p-TLS connection

	queue<Packet *> pkt_storage;	// FiFo-Queue to buffer packets until TLS connection is established

	unsigned int traffic_cnt;
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

	void shutdown_tls();
	bool is_shutdown(); // Ask TLS, if shutdown is done

	void add_handlers();

	unsigned int get_traffic_cnt();

private:
	EtherAddress _me;
	EtherAddress _ks_addr;

	String _role;
	enum role_t role;
	String keydir;

	SSL_CTX *ctx;

	com_obj *curr;

	SSL_SESSION *session; 		// Used by client for session resumption

	// Server variable: Store the tls-conn for every communicating node
	HashMap<EtherAddress, com_obj*> com_table;

	void encrypt(Packet *p); 	// send application data
	void decrypt(); 			// receive application data

	void start_ssl();

	bool do_handshake();

	void store_data(Packet *p);
	void clear_pkt_storage();
	void snd_stored_data();

	int snd_data();
	void rcv_data(Packet *p);

	void print_err();
	void print_state();
};

// For now, this functions is not integrable because
// SSL_CTX_set_default_passwd_cb needs a function pointer. But
// class functions do not provide static function pointers.
int pem_passwd_cb(char *buf, int size, int rwflag, void *password);



CLICK_ENDDECLS
#endif /* TLS_HH_ */
