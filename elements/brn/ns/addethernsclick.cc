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
 * addethernsclick.{cc,hh} -- prepends an 802.3 header to a given 802.11 packets (workaround for Nsclick)
 *
 */

#include <click/config.h>
#include "addethernsclick.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <clicknet/llc.h>
CLICK_DECLS

AddEtherNsclick::AddEtherNsclick()
  : _debug(true)
{
}

AddEtherNsclick::~AddEtherNsclick()
{
}

int
AddEtherNsclick::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  _strict = false;
  if (cp_va_kparse(conf, this, errh,
		  /* not required */
		  cpKeywords,
		  "DEBUG", cpInteger, "Debug", &_debug,
		  "STRICT", cpBool, "strict header check", &_strict,
		  cpEnd) < 0)
    return -1;
  return 0;
}

/*
 * Methods receives an incoming 802.11 frame and returns it encapusulated within a 802.3 frame.
 */
Packet *
AddEtherNsclick::simple_action(Packet *p)
{
  EtherAddress bssid;
  EtherAddress src;
  EtherAddress dst;
  struct click_wifi *w = (struct click_wifi *) p->data();

  uint8_t dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;

  switch (dir) {
  case WIFI_FC1_DIR_NODS:
    if(_debug)
      click_chatter(" $$NODS\n");
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    if(_debug)
      click_chatter(" $$TODS\n");
    // if we add an ethernet header to a 802.11 packet from a station to an access point,
    // the src is the second address and the dst is the first address (the access point)
    bssid = EtherAddress(w->i_addr3); 
    src = EtherAddress(w->i_addr2);
    dst = EtherAddress(w->i_addr1);
    break;
  case WIFI_FC1_DIR_FROMDS:
    if(_debug)
      click_chatter(" $$FROMDS\n");
    // if we add an ethernet header to a 802.11 packet from an access point to a station,
    // the src is the second address and the dst is the first address (the access point)
    dst = EtherAddress(w->i_addr1);
    bssid = EtherAddress(w->i_addr3);
    src = EtherAddress(w->i_addr2);
    break;
  case WIFI_FC1_DIR_DSTODS:
    if(_debug)
      click_chatter(" $$DSTODS\n");
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
  default:
    if (_strict) {
      if(_debug)
        click_chatter("%{element}: invalid dir %d\n",
		    this,
		    dir);
      p->kill();
      return 0;
    }
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
  }

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }

  struct click_llc *llc = (struct click_llc *) (p_out->data() + sizeof(click_wifi));
  uint16_t ether_type = llc->llc_un.type_snap.ether_type;

  p_out = p_out->push(sizeof(click_ether));
  if (!p_out) {
    return 0;
  }

  click_ether *p_out_ether = (click_ether *) p_out->data();

  memcpy(p_out_ether->ether_dhost, dst.data(), 6);
  memcpy(p_out_ether->ether_shost, src.data(), 6);
  //memcpy(w->) + 12, &ether_type, 2);
  p_out_ether->ether_type = htons(ether_type);

  if(_debug)
    click_chatter(" %s -> %s\n", EtherAddress(p_out_ether->ether_shost).unparse().c_str(),
        EtherAddress(p_out_ether->ether_dhost).unparse().c_str());

  if (_debug) {
    click_chatter("%{element}: dir %d src %s dst %s bssid %s\n",
		  this, dir, src.unparse().c_str(), dst.unparse().c_str(), bssid.unparse().c_str());
  }

  return p_out;
}


enum {H_DEBUG};

static String 
AddEtherNsclick_read_param(Element *e, void *thunk)
{
  AddEtherNsclick *td = (AddEtherNsclick *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
	return String(td->_debug) + "\n";
    default:
      return String();
    }
}
static int 
AddEtherNsclick_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  AddEtherNsclick *f = (AddEtherNsclick *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_DEBUG: {    //debug
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be an integer value between 0 and 4");
      f->_debug = debug;
      break;
    }
  }
  return 0;
}
 
void
AddEtherNsclick::add_handlers()
{
  add_read_handler("debug", AddEtherNsclick_read_param, (void *) H_DEBUG);
  add_write_handler("debug", AddEtherNsclick_write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AddEtherNsclick)
