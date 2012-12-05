/*
 * mac_filter.hh
 *
 *  Created on: 04.12.2012
 *      Author: kuehne@informatik.hu-berlin.de
 *
 *  Purpose:	Mac-Filter with black of white listing
 *  Handler:	add <mac>, delete <mac>, reset
 *
 *  How:		Using ... as storage for mac-filter list.
 */

#ifndef MAC_FILTER_HH_
#define MAC_FILTER_HH_


#include <string>

#include <click/vector.hh>
#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashmap.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class MacFilter : public BRNElement {
public:
	MacFilter();
	~MacFilter();

	const char *class_name() const { return "MacFilter"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize();

	bool add(EtherAddress addr);
	bool del(EtherAddress addr);

private:
	int _debug;

	HashMap<EtherAddress, EtherAddress> macFilterList;

	void add_handlers();
};

CLICK_ENDDECLS
#endif /* MAC_FILTER_HH_ */
