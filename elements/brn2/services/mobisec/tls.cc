/*
 * tls.cc
 *
 *  Created on: 18.04.2012
 *      Author: aureliano
 *
 *	TLS with "quiete shutdown" (due to poor reliability of network connection,
 *	even unidirectional shutdown is not really possible).
 *
 *  todo:
 *  - Uncertanty in case of renegotiation. No experience and testing here!
 *  - Uncertanty in case of packet lost. Need for tcp-wise connection.
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


CLICK_DECLS

TLS::TLS()
	: _debug(false)
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
int TLS::initialize(ErrorHandler *) {

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

	// No shutdown alert will be sent, if flag is 1. We do this because of unreliable connection.
	// "This behaviour violates the TLS standard" (see man SSL_CTX_set_quiet_shutdown).
	// NOTE: IF ANY CHANGES ARE MADE HERE, PLEASE CHECK THE switch-case in shutdown_tls()
	SSL_CTX_set_quiet_shutdown(ctx, 0);

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

	// Create SSL object (Server does this reactive on incomming ssl request)
	if (role == CLIENT) {
		curr = new com_obj(ctx, role);
		curr->dst_addr = _ks_addr;
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

	// On real and bigger testbed it's probably good to increment the tries
	for(int tries = 0; tries < 1; tries++) {
		int temp = SSL_do_handshake(curr->conn);
		snd_data(); // push data manually as we are dealing with membufs

		print_state();

		// take action based on SSL errors
		switch (SSL_get_error(curr->conn, temp)) {
		case SSL_ERROR_NONE:
			BRN_DEBUG("handshake complete");
			print_err();

			// In case that the application wanted to send
			// some data but had to do handshake before:
			if (! curr->pkt_storage.empty() )
				snd_stored_data();

			return true;
			break;
		case SSL_ERROR_WANT_READ:
			BRN_DEBUG("HANDSHAKE WANT_READ");
			//rcv_data(); // push() does automatic execution of rcv_data()
			break;
		case SSL_ERROR_WANT_WRITE:
			BRN_DEBUG("HANDSHAKE WANT_WRITE");
			//snd_data(); // already done because the BIO needs a babysitter
			break;
		case SSL_ERROR_SSL:
			BRN_DEBUG("SSL_ERROR_SSL");
			restart_tls(); // Todo: is this useful?
			print_err();
			break;
		case SSL_ERROR_SYSCALL:
			BRN_DEBUG("SSL_ERROR_SYSCALL");
			print_err();
			break;
		case SSL_ERROR_ZERO_RETURN:
			BRN_DEBUG("SSL_ERROR_ZERO_RETURN");
			print_err();
			break;
		case SSL_ERROR_WANT_CONNECT:
			BRN_DEBUG("SSL_ERROR_WANT_CONNECT"); print_err(); break;
		case SSL_ERROR_WANT_ACCEPT:
			BRN_DEBUG("SSL_ERROR_WANT_ACCEPT"); print_err(); break;
		default:
			BRN_DEBUG("UNKNOWN_SSL_ERROR -> restart tls?");
			print_err();
			return false;
		}
	}

	return false;
}


// non-blocking
void TLS::encrypt(Packet *p) {
	if(p->length() <= 0){
		BRN_ERROR("SSL_write with bufsize=0 is undefined.");
		print_err();
		return;
	}

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
	//BRN_DEBUG("processing app-data ... %d bytes", size);


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
		if (p) {
			output(1).push(p);
		} else {
			BRN_ERROR("In TLS::decrypt packet make failed.");
		}
	} else {
		BRN_ERROR("In TLS::decrypt something wrong on SSL_read");
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
	EtherAddress dst_addr = BRNPacketAnno::src_ether_anno(p);
	BRN_DEBUG("Communicating with %s", dst_addr.unparse().c_str());

	/*
	 * Dynamic SSL-object selection here:
	 * Server needs to adapt the connection, depending on whom he is communicating with
	 */
	if (role == SERVER) {
		com_obj *tmp = com_table.find(dst_addr);
		if(tmp) {
			curr = tmp;
		} else {
			curr = new com_obj(ctx, role);
			curr->dst_addr = dst_addr;
			com_table.insert(dst_addr, curr);
		}
	}

	BIO_write(curr->bioIn,p->data(),p->length());
	p->kill();

	if (do_handshake() == true
			&& SSL_read(curr->conn, NULL, 0)==0 /* read 0 bytes to help SSL_pending get a look on next SSL record*/
			&& SSL_pending(curr->conn) > 0) {
		print_err(); // Errors from decryption are detectable here, because SSL_read is doing first try.
		decrypt();
	} else if (role == CLIENT && SSL_get_shutdown(curr->conn) & SSL_RECEIVED_SHUTDOWN) {
		// Only client gets a shutdown alert at unidirectional shutdown protocol.
		// By calling SSL_read the SSL_RECEIVED_SHUTDOWN flag is set.

		print_err();

		BRN_DEBUG("Received shutdown alert from %s", curr->dst_addr.unparse().c_str());
		shutdown_tls();
	}
}


int TLS::snd_data() {
	BRN_DEBUG("Check SSL-send-buffer ... ");

	int len;
	if ( (len = BIO_ctrl_pending(curr->bioOut)) <= 0 )
		return 0; // nothing to do

	WritablePacket *p_out = Packet::make(128, NULL, len, 32);
	if(!p_out) {
		BRN_DEBUG("snd_data(): Problems creating raw packets");
		return 0;
	}

	if ( BIO_read(curr->bioOut,p_out->data(), p_out->length()) ) {
		BRN_DEBUG("Sending ssl-pkt to %s (%d bytes)", curr->dst_addr.unparse().c_str(), len);

		// Set information
		BRNPacketAnno::set_ether_anno(p_out, _me, curr->dst_addr, ETHERTYPE_BRN);

		output(0).push(p_out);
		BRN_DEBUG("data sent successfully");
	} else {
		p_out->kill();
		BRN_DEBUG("sending data probably failed");
	}

	return len;
}

/*
 * *******************************************************
 *               extra functions
 * *******************************************************
 */

/*
 * Normal implementation of unidirectional shutdown:
 * 1. Server initiates unidirectional shutdown (which returns 0) and sends a shutdown alert.
 * 2. Client receives alert and junks the ssl-object, which is done in restart_tls().
 *
 * For now we use "quiet shutdown" which sends no alert at all. Instead KDP removes
 * total ssl connection on both sides.
 *
 */
void TLS::shutdown_tls() {
	int ret;

	BRN_INFO("shutdown ssl connection");

	if (role == CLIENT) {

		ret = SSL_clear(curr->conn);
		BRN_DEBUG("ssl_clear ret = %d", ret);

	} else if (role == SERVER) {

		ret = SSL_shutdown(curr->conn);
		BRN_DEBUG("ssl_shutdown ret = %d", ret);
		print_err();

		switch (ret) {
		case 0:
			// Send unidirectional "shutdown notify" to peer
			// Todo: Get this message reliable to client otherwise
			// SSL_CTX_set_quiet_shutdown(ctx, 1) is the only way :(
			snd_data();

			//BRN_DEBUG("removing ssl connection with %s", curr->dst_addr.unparse().c_str());
			//com_table.remove(curr->dst_addr);
			//delete(curr->conn);
			ret = SSL_clear(curr->conn);
			BRN_DEBUG("ssl_clear ret = %d", ret);

			break;
		case -1:
			BRN_ERROR("shutdown failed");
			print_err();
		}

		print_state();
	}
}

void TLS::restart_tls() {

	if (role == CLIENT) {
		BRN_DEBUG("removing ssl connection with %s", curr->dst_addr.unparse().c_str());

		// Clean up ssl connection
		delete(curr);

		// Create new ssl connection
		curr = new com_obj(ctx, role);
		curr->dst_addr = _ks_addr;

		// Packet lost here.
		clear_pkt_storage();
	} else if (role == SERVER) {

		// Clean up ssl connection
		delete(curr);

		com_table.remove(curr->dst_addr);
		delete(curr->conn);
	}
}

int pem_passwd_cb(char *buf, int size, int , void *password) {
	strncpy(buf, (char *)(password), size);
	buf[size - 1] = '\0';
	return(strlen(buf));
}

static String handler_triggered_handshake(Element *, void *) {
	// TLS *tls = (TLS *)e;
	// to test this, make do_handshake public!
	// tls->do_handshake();
	return String();
}

static String handler_triggered_restart(Element *e, void *) {
	TLS *tls = (TLS *)e;
	tls->restart_tls();
	return String();
}

static String handler_triggered_shutdown(Element *e, void *) {
	TLS *tls = (TLS *)e;
	tls->shutdown_tls();
	return String();
}

static String handler_triggered_is_shutdown(Element *e, void *) {
	TLS *tls = (TLS *)e;
	tls->is_shutdown();
	return String();
}

void TLS::print_err() {
	unsigned long err;
	while ((err = ERR_get_error())) {
		BRN_ERROR("openssl error: %s", ERR_error_string(err, NULL));
	}
}

void TLS::print_state() {
	BRN_INFO("openssl state: %s", SSL_state_string_long(curr->conn));
}

bool TLS::is_shutdown()  {
	return SSL_get_shutdown(curr->conn) & SSL_RECEIVED_SHUTDOWN;
}


void TLS::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("is_shutdown", handler_triggered_is_shutdown, 0);
  add_read_handler("restart", handler_triggered_restart, 0);
  add_read_handler("shutdown", handler_triggered_shutdown, 0);
  add_read_handler("handshake", handler_triggered_handshake, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(TLS)
