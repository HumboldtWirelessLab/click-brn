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
#include "elements/brn2/brnelement.hh"

#include "kdp.hh"

CLICK_DECLS

class backbone_node : public BRNElement {
public:
	backbone_node();
	~backbone_node();

	const char *class_name() const { return "backbone_node"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	void snd_kdp_req();

private:
	int _debug;
	enum proto_type _protocol_type;
	int req_id;

	void handle_kdp_reply(Packet *);

	void add_handlers();
};

static String handler_triggered_request(Element *e, void *thunk);


CLICK_ENDDECLS
#endif /* BACKBONE_NODE_HH_ */
