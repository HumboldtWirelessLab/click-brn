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
	void run_timer(Timer* );

	void snd_kdp_req();

private:
	BRN2NodeIdentity *_me;
	Element *_wepencap;
	Element *_wepdecap;
	int _debug;

	// Parameters to control the refreshing of key material
	int _start_time;
	Timer _timer;

	int req_id;

	keymanagement keyman;

	// Parameter to define the security level
	enum proto_type _protocol_type;
	int _key_timeout;

	//todo: kmm-objekt erstellen
	void handle_kdp_reply(Packet *);

	void add_handlers();
};

static String handler_triggered_request(Element *e, void *thunk);


CLICK_ENDDECLS
#endif /* BACKBONE_NODE_HH_ */
