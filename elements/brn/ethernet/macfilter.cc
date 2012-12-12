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
#include "elements/brn/routing/identity/brn2_device.hh"

CLICK_DECLS

MacFilter::MacFilter()
{
	BRNElement::init();
}

MacFilter::~MacFilter() {
}

int MacFilter::configure(Vector<String> &conf, ErrorHandler *errh) {
if (cp_va_kparse(conf, this, errh,
		"DEVICE", cpkP+cpkM, cpElement, &_device,
		"USE_FRAME", cpkP, cpString, &_use_frame,
		"DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
		cpEnd) < 0)
		return -1;

	/*
	 * Decide from which frame type to get EtherAddress.
	 * If _use_frame is defined explicitly, then use it.
	 * Else use _device to figure out, which frame to get the address.
	 * Or else use a default, which is to take out the address from packet anno.
	 */
	if(_use_frame == "anno") {
		use_frame = 1;
	} else if(_use_frame == "wifi") {
		use_frame = 2;
	} else if(_use_frame == "ether") {
		use_frame = 3;
	} else if(!_use_frame) {
		if(_device != NULL) {

			BRN2Device *device = (BRN2Device *) _device;
			uint32_t type = device->getDeviceType();

			switch(type) {
			case DEVICETYPE_WIRED:
				use_frame = 3;
				break;
			case DEVICETYPE_WIRELESS:
				use_frame = 2;
				break;
			case DEVICETYPE_VIRTUAL:
			case DEVICETYPE_UNKNOWN:
			default:
				use_frame = 0;
				break;
			}

		} else { //default
			use_frame = 0;
		}
	}



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

	EtherAddress src_addr;
	struct click_wifi *w;

	switch(use_frame) {
	case 1:
		// Get data from Packet Anno
		src_addr = BRNPacketAnno::src_ether_anno(p);
		break;
	case 2:
		// Get data from wifi frame
		w = (struct click_wifi *) p->data();
		src_addr = EtherAddress(w->i_addr2);
		break;
	default:
		// Get data from Packet Anno
		src_addr = BRNPacketAnno::src_ether_anno(p);
		break;
	}

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
