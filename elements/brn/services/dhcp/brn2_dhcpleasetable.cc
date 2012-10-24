/*
 * leasetable.{cc,hh} -- track dhcp leases
 * John Bicket
 *
 * Copyright (c) 2005 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/etheraddress.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/straccum.hh>
#include "brn2_dhcpleasetable.hh"

CLICK_DECLS

BRN2DHCPLeaseTable::BRN2DHCPLeaseTable()
{
  BRNElement::init();
}

BRN2DHCPLeaseTable::~BRN2DHCPLeaseTable()
{
}

int
BRN2DHCPLeaseTable::configure( Vector<String> &conf, ErrorHandler *errh )
{
  if (cp_va_kparse(conf, this, errh,
		   "DEBUG", cpkP, cpInteger, &_debug,
		   cpEnd) < 0 ) {
	  return -1;
  }
  return 0;
}

void *
BRN2DHCPLeaseTable::cast(const char *n)
{
	if (strcmp(n, "BRN2DHCPLeaseTable") == 0)
		return (BRN2DHCPLeaseTable *)this;
	return 0;
}

Lease *
BRN2DHCPLeaseTable::rev_lookup(EtherAddress eth)
{
    if (HashTable<EtherAddress, IPAddress>::iterator it = _ips.find(eth))
	return lookup(it.value());
    else
	return 0;
}

Lease *
BRN2DHCPLeaseTable::lookup(IPAddress ip)
{
    if (LeaseMap::iterator it = _leases.find(ip))
	return &it.value();
    else
	return 0;
}

void
BRN2DHCPLeaseTable::remove(EtherAddress eth)
{
    if (Lease *l = rev_lookup(eth)) {
	IPAddress ip = l->_ip;
	_leases.erase(ip);
	_ips.erase(eth);
    }
}

bool
BRN2DHCPLeaseTable::insert(Lease l) {
	IPAddress ip = l._ip;
	EtherAddress eth = l._eth;
	_ips.set(eth, ip);
	_leases.set(ip, l);
	return true;
}

enum {H_LEASES};
String
BRN2DHCPLeaseTable::read_handler(Element *e, void *thunk)
{
	BRN2DHCPLeaseTable *lt = (BRN2DHCPLeaseTable *)e;
	switch ((uintptr_t) thunk) {
	case H_LEASES: {
		StringAccum sa;
    sa << "<dhcpleases count=\"" << lt->_leases.size() << "\" time=\"" << Timestamp::now().unparse() << "\" >\n";
		for (LeaseIter iter = lt->_leases.begin(); iter.live(); iter++) {
			Lease l = iter.value();
      sa << "\t<client ip=\"" << l._ip << "\" mac=\"" << l._eth;
			sa << "\" start=\"" << l._start.sec() << "\" end=\"" << l._end.sec();
      sa << "\" duration=\"" << l._duration.sec();
      sa << "\" time_left=\"" << (l._end - Timestamp::now()).sec() << "\" />\n";
		}
    sa << "</dhcpleases>\n";
    return sa.take_string();
	}
	default:
		return String();
	}
}
void
BRN2DHCPLeaseTable::add_handlers()
{
  BRNElement::add_handlers();

	add_read_handler("leases", read_handler, (void *) H_LEASES);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DHCPLeaseTable)

