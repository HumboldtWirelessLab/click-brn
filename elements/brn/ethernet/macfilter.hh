/*
 * macfilter.hh
 *
 *  Created on: 04.12.2012
 *      Author: kuehne@informatik.hu-berlin.de
 *
 *  Purpose:	Mac-Filter using blacklisting
 *  Handler:	add <mac>, del <mac>
 *
 *  Input0:		Receive from device.
 *  Output0:	Pass through.
 */

#ifndef MAC_FILTER_HH_
#define MAC_FILTER_HH_

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

	HashMap<EtherAddress, int> macFilterList;
        
        String stats();

private:
	Element *_device;

	String _use_frame;
	int use_frame;

	void add_handlers();
};

CLICK_ENDDECLS
#endif /* MAC_FILTER_HH_ */
