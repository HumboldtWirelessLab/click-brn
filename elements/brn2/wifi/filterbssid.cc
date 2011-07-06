/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * filterbssid.{cc,hh}
 * based on John Bicket's openauthrequester.{cc,hh}
 *
 */

#include <click/config.h>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/llc.h>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "filterbssid.hh"

CLICK_DECLS

FilterBSSID::FilterBSSID()
  : _winfo(0)
{
  BRNElement::init();
}

FilterBSSID::~FilterBSSID()
{
}

int
FilterBSSID::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _active = true;
  if (cp_va_kparse(conf, this, errh,
      "ACTIVE", cpkP+cpkM, cpBool, &_active,
      "DEBUG", cpkP+cpkM, cpInteger, &_debug,
      "WIRELESS_INFO", cpkP+cpkM, cpElement, &_winfo,
		  cpEnd) < 0)
    return -1;

  if (!_winfo || !_winfo->cast("WirelessInfo"))
    return errh->error("WIRELESS_INFO element is not a WirelessInfo");

  return 0;
}

void 
FilterBSSID::push(int, Packet *p)
{
  EtherAddress dst;
  EtherAddress src;
  EtherAddress bssid;
 

  uint8_t dir;
  struct click_wifi *w = (struct click_wifi *) p->data();

  // TODO why click_llc??
  if (p->length() < sizeof(struct click_wifi) + sizeof(struct click_llc)) {
    BRN_WARN("got too small packet.");
    checked_output_push(2, p);
    return;
  }

  dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;

  switch (dir) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    dst = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_FROMDS:
    dst = EtherAddress(w->i_addr1);
    bssid = EtherAddress(w->i_addr2);
    src = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_DSTODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
  default:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
  }

  /* test if packet's BSSID matches the WirelessInfo ones */
  if (_winfo->_bssid == bssid)
  {
    output(0).push(p);
    return; 
  }

  // If not active, simply print out a debug message
  if (!_active) 
  {
    BRN_WARN("packet with wrong bssid %s", bssid.unparse().c_str());

    output(0).push(p);
    return; 
  }

  BRN_INFO("packet with bssid %s send to output 1", bssid.unparse().c_str());
  checked_output_push(1, p);
}


enum {H_ACTIVE};

static String 
FilterBSSID_read_param(Element *e, void *thunk)
{
  FilterBSSID *td = (FilterBSSID *)e;
  switch ((uintptr_t) thunk) {
  case H_ACTIVE:
    return String(td->_active) + "\n";
  default:
    return String();
  }
}
static int 
FilterBSSID_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  FilterBSSID *f = (FilterBSSID *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
  case H_ACTIVE: {    //debug
    bool active;
    if (!cp_bool(s, &active)) 
      return errh->error("active parameter must be boolean");
    f->_active = active;
    break;
  }
  }
  return 0;
}
 
void
FilterBSSID::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("active", FilterBSSID_read_param, (void *) H_ACTIVE);
  add_write_handler("active", FilterBSSID_write_param, (void *) H_ACTIVE);
}


#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
EXPORT_ELEMENT(FilterBSSID)
