/*
 * MacFilter.cc
 *
 *  Created on: 04.12.2012
 *      Author: kuehne@informatik.hu-berlin.de
 *
 *  Specification:
 *  			The mac filter is an active part of the
 *  			network stack. It receives packets from the device,
 *  			uses the ethernet address to check, if it is black-
 *  			listed.
 *  			If blacklisted, than discard packet.
 *  			Else, forward packet.
 *  			The blacklist will be controlled by a handler.
 *
 *  Input0:		Receive from device.
 *  Output0:	Pass through.
 *
 */


#include <click/config.h>
#include <click/element.hh>
#include <click/confparse.hh>
#include <clicknet/wifi.h>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"

#include "macfilter.hh"

CLICK_DECLS

MacFilter::MacFilter()
{
	BRNElement::init();
}

MacFilter::~MacFilter() {
}

int MacFilter::configure(Vector<String> &conf, ErrorHandler *errh) {
if (cp_va_kparse(conf, this, errh,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	return 0;
}

int MacFilter::initialize() {

	return 0;
}

void MacFilter::push(int port, Packet *p) {

	if (port != 0) {
		p->kill();
		return;
	}

  //EtherAddress src_addr = BRNPacketAnno::src_ether_anno(p);

  struct click_wifi *w = (struct click_wifi *) p->data();
  EtherAddress src_addr = EtherAddress(w->i_addr2);

	if (macFilterList.findp(src_addr) != NULL) {
		BRN_DEBUG("filtering mac");
		p->kill();
		return;
	} else {
		// Pass through
		output(0).push(p);
	}

}

bool MacFilter::add(EtherAddress addr) {

	// Check for uniqueness
	if (macFilterList.findp(addr) == NULL) {
		if (macFilterList.insert(addr,addr)) {
			BRN_DEBUG("new mac filter entry");
			return true;
		} else {
			BRN_ERROR("inserting mac filter entry failed");
			return false;
		}
	} else {
		BRN_DEBUG("mac filter entry already inserted");
		return false;
	}
}

bool MacFilter::del(EtherAddress addr) {

	// Implicit check if in hashmap
	return macFilterList.erase(addr);
}

enum {H_ADD_MAC, H_DEL_MAC};

/*
static String MacFilter_read_param(Element *e, void *thunk) {
	MacFilter *mf = (MacFilter *)e;

	switch((uintptr_t) thunk) {
	default:
		break;
	}

	return String();
}
*/

static int MacFilter_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *) {

	MacFilter *mf = (MacFilter *)e;
	String s = cp_uncomment(in_s);

	EtherAddress mac;

	// Check for valid input
	if (!cp_ethernet_address(s, &mac) ) {
		click_chatter("Not a valid ethernet address: %s", s.c_str());
		return -1;
	}

	switch((intptr_t)vparam) {
   case H_ADD_MAC:
		mf->add(mac);
		break;
   case H_DEL_MAC:
		mf->del(mac);
		break;
	default:
		break;
	}

	return 0;
}


void MacFilter::add_handlers() {

	BRNElement::add_handlers();

  add_write_handler("add", MacFilter_write_param, (void *)H_ADD_MAC);
  add_write_handler("del", MacFilter_write_param, (void *)H_DEL_MAC);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MacFilter)
