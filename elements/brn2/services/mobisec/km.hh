/*
 * km.hh -- Key Managament
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef KM_HH_
#define KM_HH_


#include <string>
#include <click/element.hh>
#include <click/confparse.hh>
#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class km : public BRNElement {
public:
	km();
	~km();

	const char *class_name() const { return "km"; }
	const char *port_count() const { return "0/0"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler* errh);

	// The crypto_generator will be executed periodically by the key server
	void crypto_generator(); // generates new key lists, saves old for a little time, installs new keys

private:
	int _debug;
};

CLICK_ENDDECLS
#endif /* KM_HH_ */
