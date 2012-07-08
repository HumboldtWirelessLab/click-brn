/*
 * tls.cc
 *
 *  Created on: 18.04.2012
 *      Author: aureliano
 *
 *  todo:
 *  - Uncertanty in case of renegotiation. No experience and testing here!
 *  - Uncertanty in case of packet lost. Need for tcp-wise connection.
 *  - Eliminate restart_timer, when having reliable tcp-connection
 *
 *  Usefull commands:
 *  - openssl req -new -newkey rsa:2028 -days 9999 -x509 -out ca.pem
 *    to create a ca cert
 *  - more info on: man ca
 */
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <queue>

#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/packet.hh>
#include <click/glue.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "tls.hh"

#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BACKOFF_TLS_RETRY 3

CLICK_DECLS

TLS::TLS()
	: _debug(false),
	  restart_timer(restart_trigger, this)
{
	BRNElement::init();
}

TLS::~TLS() {
	for (HashMap<EtherAddress, com_obj*>::const_iterator it = com_table.begin(); it.live(); it++) {
		it.value()->~com_obj();
	}
	com_table.clear();

	ERR_free_strings();
	SSL_CTX_free(ctx);
}


int TLS::configure(Vector<String> &conf, ErrorHandler *errh) {

	if (cp_va_kparse(conf, this, errh,
		"ETHERADDRESS", cpkP, cpEthernetAddress, &_me,
		"KEYSERVER", cpkP, cpEthernetAddress, &_ks_addr,
		"ROLE", cpkP, cpString, &_role,
		"KEYDIR", cpkP, cpString, &keydir,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	if(_role == "SERVER") {
		role = SERVER;
		BRN_INFO("Recognized as TLS-SERVER");
	} else if (_role == "CLIENT") {
		role = CLIENT;
		BRN_INFO("Recognized as TLS-CLIENT");
	} else {
		BRN_ERROR("no role specified");
		return -1;
	}

	return 0;
}

// called just before router is placed online
int TLS::initialize(ErrorHandler* errh) {

	SSL_load_error_strings();
	ERR_load_BIO_strings();
	SSL_library_init();

	// Check for "openssl 0.9.8 release" when using SSL_CTX_new
	#if (OPENSSL_VERSION_NUMBER & 0xfffff000) == 0x00908000
		SSL_METHOD *meth;
	#else
		const SSL_METHOD *meth;
	#endif

    meth = (role==CLIENT)? TLSv1_client_method() : TLSv1_server_method();
	ctx = SSL_CTX_new(meth);
	if (!ctx) {print_err(); return -1;}

	char password[] = "test";
	SSL_CTX_set_default_passwd_cb(ctx, &pem_passwd_cb); //passphrase for both the same
	SSL_CTX_set_default_passwd_cb_userdata(ctx, password);

	// used following cmd to get list of correct cipher lists
	// $ openssl ciphers -tls1 "aRSA:AES:-kEDH:-ECDH:-SRP:-PSK:-NULL:-EXP:-MD5:-DES"
	if(!SSL_CTX_set_cipher_list(ctx, "RC4-SHA")) {
		print_err();
		return -1;
	}

	char *dir = new char[512];

	if(role==CLIENT) {
		sprintf( dir, "%s%s", keydir.c_str(), "certs/client2.pem" );
		SSL_CTX_use_certificate_file(ctx, dir , SSL_FILETYPE_PEM);
		sprintf( dir, "%s%s", keydir.c_str(), "certs/key2.pem" );
		SSL_CTX_use_RSAPrivateKey_file(ctx, dir, SSL_FILETYPE_PEM);

		sprintf( dir, "%s%s", keydir.c_str(), "certs/demoCA/cacert2.pem" );
		if (!SSL_CTX_load_verify_locations(ctx, dir,NULL)) {
			print_err();
			return -1;
		}
	} else if(role==SERVER) {
		sprintf( dir, "%s%s", keydir.c_str(), "certs/demoCA/cacert2.pem" );
		SSL_CTX_use_certificate_file(ctx, dir, SSL_FILETYPE_PEM);
		sprintf( dir, "%s%s", keydir.c_str(), "certs/demoCA/private/cakey2.pem" );
		SSL_CTX_use_RSAPrivateKey_file(ctx, dir, SSL_FILETYPE_PEM);
	}

	if(!SSL_CTX_check_private_key(ctx)) {
		BRN_ERROR("Gooong... Interesting, instead of a private key you found a chinese instrument ;)");
		print_err();
		return -1;
	}

	if (role == CLIENT) {
		curr = new com_obj(ctx, role);
		curr->sender_addr = _ks_addr;

		restart_timer.initialize(this);
	}



	BRN_INFO("initialized");

	return 0;
}

void TLS::push(int port, Packet *p) {

	if (port == 0) { // data from network
		BRN_DEBUG("port %d: rcv ssl-record (%d bytes) <<<", port, p->length());
		rcv_data(p);
	} else if (port == 1) { // data to encrypt
		BRN_DEBUG("port %d: encrypting %d bytes >>>", port, p->length());
		encrypt(p);
	} else {
		BRN_DEBUG("port %d: oops !!", port, p->length());
		p->kill();
	}
}

/*
 * *******************************************************
 *               private functions
 * *******************************************************
 */


bool TLS::do_handshake() {
	BRN_DEBUG("check out handshake ...");

	for(int tries = 0; tries < 3; tries++) {
		int temp = SSL_do_handshake(curr->conn);
		snd_data(); // push data manually as we are dealing with membufs

		// take action based on SSL errors
		switch (SSL_get_error(curr->conn, temp)) {
		case SSL_ERROR_NONE:
			BRN_DEBUG("handshake complete");
			if (role == CLIENT) restart_timer.clear();

			// In case that the application wanted to send
			// some data but had to do handshake before:
			if (! curr->pkt_storage.empty() )
				snd_stored_data();

			return true;
			break;
		case SSL_ERROR_WANT_READ:
			BRN_DEBUG("HANDSHAKE WANT_READ");
			//rcv_data(); // useless??
			break;
		case SSL_ERROR_WANT_WRITE:
			BRN_DEBUG("HANDSHAKE WANT_WRITE");
			//snd_data();
			break;
		case SSL_ERROR_SSL:
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_ZERO_RETURN:
		default:
			print_err();
			return false;
		}
	}

	if (role == CLIENT)
		if(restart_timer.expiry().msecval() <= Timestamp::now().msecval())
			restart_timer.schedule_after_s(BACKOFF_TLS_RETRY);

	return false;
}


// non-blocking
void TLS::encrypt(Packet *p) {
	if(p->length() <= 0){
		BRN_ERROR("SSL_write with bufsize=0 is undefined.");
		print_err();
		return;
	}

	// todo: Empfänger und Absender zwischenspeichern

	int ret = SSL_write(curr->conn,p->data(),p->length());
	if(ret>0) {
		BRN_DEBUG("SSL ready... sending encrypted data");
		snd_data();
	} else {

		BRN_DEBUG("SSL not ready... storing data while doing handshake");
		store_data(p);

		do_handshake();
	}
}

// non-blocking
void TLS::decrypt() {
	int size = SSL_pending(curr->conn);
	//BRN_DEBUG("processing app-data ...");


	data_t *data = (data_t *)malloc(size);
	if(!data) {
		BRN_ERROR("SSL_CONN::receive no memory allocated.");
		print_err();
		return;
	}

	int ret = SSL_read(curr->conn,data,size); // data received in records of max 16kB

	// If SSL_read was successful receiving full records, then ret > 0.
	if(ret > 0) {
		// BRN_DEBUG("...... decrypted");
		// Push decrypted message to the next element.
		WritablePacket *p = Packet::make(data, size);
		output(1).push(p);
	} else {
		BRN_DEBUG("...... decryption failed");
	}

	free(data);
}

void TLS::clear_pkt_storage() {
	BRN_DEBUG("Clearing pkt storage");
	/*  (This seems computationally inefficient to me) */
	while(!curr->pkt_storage.empty()) {
		curr->pkt_storage.front()->kill();
		curr->pkt_storage.pop();
	}
}

void TLS::store_data(Packet *p) {
	curr->pkt_storage.push(p);
}

void TLS::snd_stored_data() {
	while(! curr->pkt_storage.empty() ) {
		Packet *p = curr->pkt_storage.front();
		encrypt(p);
		curr->pkt_storage.pop();
	}
}

void TLS::rcv_data(Packet *p) {
	EtherAddress sender_addr = BRNPacketAnno::src_ether_anno(p);
	BRN_DEBUG("Communicating with %s", sender_addr.unparse().c_str());

	/*
	 * Dynamic SSL-object selection here:
	 * Server needs to adapt the connection, depending on whom he is communicating with
	 */
	if (role == SERVER) {
		com_obj *tmp = com_table.find(sender_addr);
		if(tmp) {
			curr = tmp;
		} else {
			curr = new com_obj(ctx, role);
			curr->sender_addr = sender_addr;
			com_table.insert(sender_addr, curr);
		}

		/* Painting-technique gives the server packet oriented control. */
		uint8_t color = static_cast<int>(p->anno_u8(PAINT_ANNO_OFFSET));
		curr->wep_state = (color == 42) ? false : true;
	}



	BIO_write(curr->bioIn,p->data(),p->length());
	p->kill();

	// If handshake is complete we assume
	// that incomming data is for application
	if (do_handshake() == true
			&& SSL_read(curr->conn, NULL, 0)==0 /* read 0 bytes to help SSL_pending get a look on next SSL record*/
			&& SSL_pending(curr->conn) > 0) {
		decrypt();
	}
}


int TLS::snd_data() {
	BRN_DEBUG("Check SSL-send-buffer ... ");

	int len;
	if ( (len = BIO_ctrl_pending(curr->bioOut)) <= 0 )
		return 0; // nothing to do

	WritablePacket *p = Packet::make(128, NULL, len, 32);
	if(!p) {
		BRN_DEBUG("snd_data(): Problems creating raw packets");
		return 0;
	}

	if ( BIO_read(curr->bioOut,p->data(), p->length()) ) {
		BRN_DEBUG("Sending ssl-pkt to %s (%d bytes)", curr->sender_addr.unparse().c_str(), len);

		// Pack into BRN-Pkt
		BRNPacketAnno::set_ether_anno(p, _me, curr->sender_addr, ETHERTYPE_BRN);
	    WritablePacket *p_out = BRNProtocol::add_brn_header(p, BRN_PORT_FLOW, BRN_PORT_FLOW, 5, DEFAULT_TOS);


	    /* Painting-technique gives the server packet oriented control. */
	    if (role == SERVER) {
			if(curr->wep_state == false) {
				uint8_t color = 42;
				p_out->set_anno_u8(PAINT_ANNO_OFFSET, color);
			}
	    }

		output(0).push(p_out);
		BRN_DEBUG("data sent successfully");
	} else {
		p->kill();
		BRN_DEBUG("sending data probably failed");
	}

	return len;
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */

// Todo: Eliminate this, when having reliable tcp-connection
void TLS::restart_tls() {
	BRN_DEBUG("Timeout -> restart tls conn");
	SSL_clear(curr->conn);
	do_handshake();
}

int pem_passwd_cb(char *buf, int size, int rwflag, void *password) {
	strncpy(buf, (char *)(password), size);
	buf[size - 1] = '\0';
	return(strlen(buf));
}

static String handler_triggered_handshake(Element *e, void *thunk) {
	TLS *tls = (TLS *)e;
	// to test this, make do_handshake public!
	// tls->do_handshake();
	return String();
}

void TLS::print_err() {
	BRN_ERROR("%s" , ERR_error_string(ERR_get_error(), NULL));
}


void TLS::add_handlers()
{
  BRNElement::add_handlers();


  add_read_handler("handshake", handler_triggered_handshake, NULL);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(TLS)
