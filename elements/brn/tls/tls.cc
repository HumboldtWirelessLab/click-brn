/*
 * tls.cc
 *
 *  Created on: 18.04.2012
 *      Author: kuehne@informatik.hu-berlin.de
 *
 * TLS with "quiete shutdown" (due to poor reliability of network connection,
 * even unidirectional shutdown is not really possible) and session resumption (ticketing).
 *
 * Session resumption explained in HP paper on openssl setup.
 * This module does session resumption. Thus the client saves session
 * and restores it.
 *
 * For the SSL operations this click module is non-blocking, because
 * it uses non-blocking BIOs. To encrypt data the BIO we proactively
 * send the data on our own initiative. To decrypt data we expect the
 * push function to initiate the decryption process automatically.
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

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"

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
		/*"KEYSERVER", cpkP, cpEthernetAddress, &_ks_addr,*/ // obsolete,
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
	SSL_CTX_set_quiet_shutdown(ctx, 1);

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

	// I dont know if TLS is already doing this, but I need mutual authentication. Maybe it's
	// default by providing a client-cert to openssl.
	// Todo: Needs some testing with fake certificate and cert-chain on client
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER & SSL_VERIFY_CLIENT_ONCE, NULL);

	BRN_INFO("initialized");

	return 0;
}

void TLS::push(int port, Packet *p) {
	com_obj *tmp;
	EtherAddress dst_addr;
	switch(port) {
	case 0: /*data from network*/	dst_addr = BRNPacketAnno::src_ether_anno(p); break;
	case 1: /*data to encrypt*/		dst_addr = BRNPacketAnno::dst_ether_anno(p); break;
	default: break;
	}

	// **************** Dynamic SSL-object selection ******************
	// ****************************************************************
	tmp = com_table.find(dst_addr);
	switch(role) {
	case SERVER:
		if(tmp) {
			curr = tmp;
			if (is_shutdown()) {
				BRN_DEBUG("connection is shutdown before handshake! ===> refresh ssl and try session resumption");
				curr->refresh();
				print_err();
				print_state();
			} else {
				BRN_DEBUG("is not shutdown");
			}
		} else {
			curr = new com_obj(ctx, role);
			curr->dst_addr = dst_addr;
			com_table.insert(dst_addr, curr);
		}
		break;
	case CLIENT:
		if(tmp) {
			curr = tmp;
		} else {
			curr = new com_obj(ctx, role);
			curr->dst_addr = dst_addr;
			com_table.insert(dst_addr, curr);
		}
		break;
	default:
		BRN_ERROR("Handle tls-packet with undefined role");
		break;
	}
	// ****************************************************************
	// ****************************************************************

	switch(port){
	case 0: // data from network
		BRN_DEBUG("Communicating with %s", dst_addr.unparse().c_str());
		BRN_DEBUG("port %d: rcv ssl-record (%d bytes) <<<", port, p->length());
		rcv_data(p);
		break;
	case 1: // data to encrypt
		BRN_DEBUG("port %d: encrypting %d bytes >>>", port, p->length());

		if(role == CLIENT) start_ssl(); // todo: stupid workaround...

		encrypt(p);
		break;
	default:
		BRN_DEBUG("port %d: oops !!", port, p->length());
		p->kill();
		break;
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
			//snd_data(); // already done because we use a memory BIO which we have to check frequently
			break;
		case SSL_ERROR_SSL:
			BRN_DEBUG("SSL_ERROR_SSL");
			shutdown_tls();
			break;
		case SSL_ERROR_SYSCALL:
			BRN_DEBUG("SSL_ERROR_SYSCALL");
			break;
		case SSL_ERROR_ZERO_RETURN:
			BRN_DEBUG("SSL_ERROR_ZERO_RETURN");
			shutdown_tls();
			break;
		case SSL_ERROR_WANT_CONNECT:
			BRN_DEBUG("SSL_ERROR_WANT_CONNECT"); break;
		case SSL_ERROR_WANT_ACCEPT:
			BRN_DEBUG("SSL_ERROR_WANT_ACCEPT"); break;
		default:
			BRN_DEBUG("UNKNOWN_SSL_ERROR -> shutdown tls");
			shutdown_tls();
		}

		print_err();
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

		BRN_DEBUG("SSL not ready... storing data while trying to ssl-connect");
		store_data(p);

		do_handshake();
	}
}

// non-blocking
void TLS::decrypt() {
	int size = SSL_pending(curr->conn);

	data_t *data = (data_t *)malloc(size);
	if(!data) {
		BRN_ERROR("SSL_CONN::receive no memory allocated.");
		print_err();
		return;
	}

	int ret = SSL_read(curr->conn,data,size); // data received in records of max 16kB

	// If SSL_read was successful receiving full records, then ret > 0.
	if(ret > 0) {

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

	// Server must not start connection, if he does, ignore.
	// Todo: If reliable transport, check this again, if necessary..
	if(role == CLIENT && is_shutdown()) {
		p->kill();
		return;
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

		// Todo: If reliable transport exists, shutdown alert can be used
		// BRN_DEBUG("Received shutdown alert from %s", curr->dst_addr.unparse().c_str());
		// shutdown_tls();
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

void TLS::start_ssl() {
	if (role == CLIENT) {
		if (is_shutdown()) {
			BRN_DEBUG("connection is shutdown before handshake! ===> refresh ssl");
			curr->refresh();

			BRN_DEBUG("checkout timeout of session: %d sec", SSL_SESSION_get_timeout(session));

			// Session resumption has to be checked before handshake
			if (SSL_set_session(curr->conn, session)) {
				BRN_DEBUG("set ssl session successfully");
			}

			print_err();
			print_state();

		} else {
			BRN_DEBUG("is not shutdown, connection seams alive");
		}
	}
}

/*
 * Normal implementation of unidirectional shutdown:
 * 1. Server initiates unidirectional shutdown (which returns 0) and sends a shutdown alert.
 * 2. Client receives alert and shuts tls down.
 *
 * For now we use "quiet shutdown" which sends no alert at all. Instead KDP shuts down
 * tls on both sides
 */
void TLS::shutdown_tls() {

	if (role == CLIENT) {
		if (! (session = SSL_get1_session(curr->conn))) {
			BRN_DEBUG("no ssl session available");
		} else {
			BRN_DEBUG("ssl session saved");
		}

		// Clean up.
		clear_pkt_storage();
	}

	// Todo: Change when having reliable transport
	// ret = SSL_shutdown(curr->conn);
	// BRN_DEBUG("ssl_shutdown ret = %d", ret);
	// No send or receive of shutdown alert at all. Just set flags.
	SSL_set_shutdown(curr->conn, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
	BRN_DEBUG("ssl set shutdown");

	print_state();
}

unsigned int TLS::get_traffic_cnt() {

	return curr->traffic_cnt;
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

static String handler_get_traffic_cnt(Element *e, void *) {
	TLS *tls = (TLS *)e;
	click_chatter("traffic_cnt: %d", tls->get_traffic_cnt());
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
	return (SSL_get_shutdown(curr->conn) > 0);
}


void TLS::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("is_shutdown", handler_triggered_is_shutdown, 0);
  add_read_handler("shutdown", handler_triggered_shutdown, 0);
  add_read_handler("handshake", handler_triggered_handshake, 0);
  add_read_handler("traffic_cnt", handler_get_traffic_cnt, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns FakeOpenSSL)
EXPORT_ELEMENT(TLS)
