/*
 * backbone_node.hh
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef BACKBONE_NODE_HH_
#define BACKBONE_NODE_HH_


#include <string>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/wifi/wepencap.hh"
#include "elements/wifi/wepdecap.hh"

#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

class BACKBONE_NODE : public BRNElement {
public:
	BACKBONE_NODE();
	~BACKBONE_NODE();

	const char *class_name() const { return "BACKBONE_NODE"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	void snd_kdp_req();

	void jmp_next_session();
	static void session_trigger(Timer *t, void *element) { ((BACKBONE_NODE *)element)->jmp_next_session(); }

	void jmp_next_epoch();
	static void epoch_trigger(Timer *t, void *element) { ((BACKBONE_NODE *)element)->jmp_next_epoch(); }

	static void kdp_trigger(Timer *t, void *element) { ((BACKBONE_NODE *)element)->snd_kdp_req(); }

private:
	BRN2NodeIdentity *_me;
	Element *_wepencap;
	Element *_wepdecap;
	int _debug;
	int _start_time;

	// Parameters to control the refreshing of key material
	Timer session_timer; 	// Controls installation of keys
	Timer epoch_timer; 		// Controls the keylist refreshing
	Timer kdp_timer;		// Controls the kdp-req sending process
	int tolerance;			// Tolerated time for kdp-req in
							// order to receive the kdp-replies in time
	int backoff; 			// Time between resending a new kdp without wep encryption
	int last_req_try;		// Save the timestamp of last kdp-request try

	int req_id;

	keymanagement keyman;
	keymanagement BUF_keyman;	// a change between two subsequent epochs need a buffering structure

	// Parameter to define the security level
	enum proto_type _protocol_type;
	int _key_timeout;

	void handle_kdp_reply(Packet *);
	void switch_wep(String);

	void add_handlers();
};

static String handler_triggered_request(Element *e, void *thunk);


CLICK_ENDDECLS
#endif /* BACKBONE_NODE_HH_ */
