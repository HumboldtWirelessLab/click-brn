/*
 * keyserver.hh
 *
 *  Created on: 27.04.2012
 *      Author: aureliano
 *
 * This module executes the KDP from the perspective of a server.
 *
 * Todo: Dieses Modul muss auch die Aufgabe übernehmen, Replay-Angriffe zu bewältigen
 */

#ifndef KEYSERVER_HH_
#define KEYSERVER_HH_


#include <string>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/confparse.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/wifi/wepencap.hh"
#include "elements/wifi/wepdecap.hh"

#include "kdp.hh"
#include "keymanagement.hh"


CLICK_DECLS

class KEYSERVER : public BRNElement {
public:
	KEYSERVER();
	~KEYSERVER();

	const char *class_name() const { return "KEYSERVER"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	void jmp_next_session();
	static void session_trigger(Timer *, void *element) { ((KEYSERVER *)element)->jmp_next_session(); }

	void jmp_next_epoch();
	static void epoch_trigger(Timer *, void *element) { ((KEYSERVER *)element)->jmp_next_epoch(); }

	void prepare_new_epoch();

private:
	int _debug;
	EtherAddress _me;
	Element *_wepencap;
	Element *_wepdecap;

	// Parameters to control the refreshing of key material
	int _start_time;
	Timer session_timer; 	// Controls installation of keys
	Timer epoch_timer; 	// Controls the keylist refreshing

	bool start_flag;

	keymanagement keyman;
	keymanagement BUF_keyman;	// a change between two subsequent epochs need a buffering structure

	// Parameter to define the security level
	enum proto_type _protocol_type;
	int _key_list_cardinality;
	int _key_timeout;

	void handle_kdp_req(Packet *p);
};

CLICK_ENDDECLS
#endif /* KEYSERVER_HH_ */
