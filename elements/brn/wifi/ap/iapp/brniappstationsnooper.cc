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
 * brniappstationsnooper.{cc,hh} -- scans promisc data packets for other client stations 
 * M. Kurth
 */
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>

#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brniapphellohandler.hh"
#include "brniappstationtracker.hh"
#include "brniappstationsnooper.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappStationSnooper::BrnIappStationSnooper() :
  _debug(BrnLogger::DEFAULT),
  _optimize(true),
  _id(NULL),
  _assoc_list(NULL),
  _hello_handler(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappStationSnooper::~BrnIappStationSnooper()
{
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationSnooper::set_optimize(bool optimize)
{
  BRN_INFO("optimization turned %s", (_optimize ? "on" : "off"));
  _optimize = optimize;
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappStationSnooper::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"AssocList element",*/ &_assoc_list,
      "STATRACK", cpkP+cpkM, cpElement, /*"StationTracker element",*/ &_sta_tracker,
      "HELLOHDL", cpkP+cpkM, cpElement, /*"HelloHandler element",*/ &_hello_handler,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_id,
      "OPTIMIZE", cpkP, cpBool,/* "Optimize",*/ &_optimize,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      cpEnd) < 0)
    return -1;

  BRN_INFO("optimization turned %s", (_optimize ? "on" : "off"));

  if (!_assoc_list || !_assoc_list->cast("BRN2AssocList")) 
    return errh->error("BRN2AssocList not specified");

  if (!_sta_tracker || !_sta_tracker->cast("BrnIappStationTracker")) 
    return errh->error("StationTracker not specified");

  if (!_hello_handler || !_hello_handler->cast("BrnIappHelloHandler")) 
    return errh->error("BrnIappHelloHandler not specified");

  if (!_id || !_id->cast("BRN2NodeIdentity"))
    return errh->error("BRN2NodeIdentity not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappStationSnooper::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationSnooper::push(int, Packet *p)
{
  if (!_optimize)
  {
    if (p)
      p->kill();
    return;
  }

  BRN_CHECK_EXPR_RETURN(NULL == p || p->length() < sizeof(struct click_wifi),
    ("invalid arguments"), if (p) p->kill();return;);

  const click_wifi* wifi = (const click_wifi*) p->data();

  BRN_CHECK_EXPR_RETURN(NULL == wifi,
    ("missing wifi header"), if (p) p->kill();return;);

  // Check frame control
  BRN_CHECK_EXPR_RETURN(WIFI_FC0_VERSION_0 != (wifi->i_fc[0] & WIFI_FC0_VERSION_MASK),
    ("invalid frame type, discard (fc[0]=0x%x).", wifi->i_fc[0]), 
    if (p) p->kill();return;);

  if (WIFI_FC0_TYPE_DATA == (wifi->i_fc[0] & WIFI_FC0_TYPE_MASK)
    &&WIFI_FC0_SUBTYPE_DATA == (wifi->i_fc[0] & WIFI_FC0_SUBTYPE_MASK))
    peek_data(p);
  else if (WIFI_FC0_TYPE_MGT == (wifi->i_fc[0] & WIFI_FC0_TYPE_MASK))
    peek_management(p);

  // Kill packet anyway
  p->kill();
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationSnooper::peek_management(Packet *p) 
{
  const click_wifi* w = (const click_wifi*) p->data();
  EtherAddress bssid;
  EtherAddress dst;
  EtherAddress src;

  uint8_t subtype = w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;
  switch(subtype) {
  case WIFI_FC0_SUBTYPE_REASSOC_REQ:
  case WIFI_FC0_SUBTYPE_ASSOC_REQ:
  case WIFI_FC0_SUBTYPE_ASSOC_RESP:
  case WIFI_FC0_SUBTYPE_REASSOC_RESP:
  case WIFI_FC0_SUBTYPE_DISASSOC:
    break;
  default:
    return;
  }

   // Check direction and extract addresses
  uint8_t dir = w->i_fc[1] & WIFI_FC1_DIR_MASK;
  switch (dir) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(w->i_addr1);
    src = EtherAddress(w->i_addr2);
    bssid = EtherAddress(w->i_addr3);
    break;
//  case WIFI_FC1_DIR_TODS:
//    bssid = EtherAddress(w->i_addr1);
//    src = EtherAddress(w->i_addr2);
//    dst = EtherAddress(w->i_addr3);
//    break;
//  case WIFI_FC1_DIR_FROMDS:
//    dst = EtherAddress(w->i_addr1);
//    bssid = EtherAddress(w->i_addr2);
//    src = EtherAddress(w->i_addr3);
//    break;
  default:
    BRN_WARN("got management packet with dir %d (subtype %x)", dir, subtype);
    return;
  }

  BRN2Device *brndev = _id->getDeviceByNumber(BRNPacketAnno::devicenumber_anno(p));
  String dev = brndev->getDeviceName();
  bool to_ds = false;

  // check bssid address
  if (dst == bssid)
    to_ds = true;
  else if (src == bssid)
    to_ds = false;
  else {
    BRN_WARN("received management frame from %s to %s with invalid bssid %s", 
      src.unparse().c_str(), dst.unparse().c_str(), bssid.unparse().c_str());
    return;
  }

  EtherAddress sta( (true == to_ds) ? src : dst);  
  EtherAddress ap( (true != to_ds) ? src : dst);  

  // look for subtype 
  switch(subtype) {
  case WIFI_FC0_SUBTYPE_REASSOC_REQ:
  case WIFI_FC0_SUBTYPE_ASSOC_REQ:
    {
      BRN_INFO("received (re)assocation request sta %s ap %s", 
        sta.unparse().c_str(), ap.unparse().c_str());
      // Do not process since it may not be successfull
    }
    break;
  case WIFI_FC0_SUBTYPE_ASSOC_RESP:
  case WIFI_FC0_SUBTYPE_REASSOC_RESP:
    {
      BRN_INFO("received (re)assocation response sta %s ap %s", 
        sta.unparse().c_str(), ap.unparse().c_str());

      // Check the status and remember it
      uint8_t *ptr = (uint8_t *) p->data() + sizeof(struct click_wifi) + 2;
      uint16_t status = le16_to_cpu( *(uint16_t *)ptr );
      if (WIFI_STATUS_SUCCESS != status) // only successfull assoc's
        break; 

      // take the state of the sta before and after the tracker was called
      BRN2AssocList::client_state state = _assoc_list->get_state(sta);
      _sta_tracker->sta_associated(sta, ap, EtherAddress(), _id->getDeviceByNumber(BRNPacketAnno::devicenumber_anno(p)), "");

      // Generate iapp hello message, if not already known
      // NOTE: since we run in promisc, we also hear others assoc's!
      BRN2AssocList::client_state new_state = _assoc_list->get_state(sta);
      if (BRN2AssocList::SEEN_OTHER == new_state
        &&new_state != state) 
      {
        // Do not send immediatly, because otherwise route requests will always collide
        // with hello messages.
        _hello_handler->schedule_hello(sta);
      }
    }
    break;
  case WIFI_FC0_SUBTYPE_DISASSOC:
    {
      BRN_INFO("received disassocation from %s", sta.unparse().c_str());

      // Remember the disassoc
      _sta_tracker->sta_disassociated(sta);
    }
    break;
  default:
    // nothing
    break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationSnooper::peek_data(Packet *p) 
{
  const click_wifi* wifi = (const click_wifi*) p->data();
  EtherAddress bssid;
  EtherAddress sta;
  EtherAddress cn;

  // Check direction and extract addresses
  uint8_t dir = wifi->i_fc[1] & WIFI_FC1_DIR_MASK;
  switch (dir) {
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(wifi->i_addr1);
    sta   = EtherAddress(wifi->i_addr2);
    cn    = EtherAddress(wifi->i_addr3);
    break;
//  case WIFI_FC1_DIR_FROMDS:
//    BRN_DEBUG("ERROR: IAPP (%s): Got packet with direction FromDS", this);
//    sta   = EtherAddress(wifi->i_addr1);
//    bssid = EtherAddress(wifi->i_addr2);
//    cn    = EtherAddress(wifi->i_addr3);

    // DO NOT consider packets with dir fromds, may not be valid information.
//    p->kill();
//    return;
  default:
    return;
  }

  BRN2Device *dev = _id->getDeviceByNumber(BRNPacketAnno::devicenumber_anno(p));

  BRN_DEBUG("seen frame for STA %s with BSSID %s and CN %s", 
    sta.unparse().c_str(), bssid.unparse().c_str(), cn.unparse().c_str());

  // Filter tx feedback
  BRN_CHECK_EXPR_RETURN((WIFI_FC1_DIR_FROMDS == (wifi->i_fc[1] & WIFI_FC1_DIR_MASK))
    && _id->isIdentical(&bssid),
    ("seen tx feedbacked frame for STA %s with BSSID %s and CN %s", 
      sta.unparse().c_str(), bssid.unparse().c_str(), cn.unparse().c_str()), return;);

  // Check for roaming
  BRN2AssocList::client_state state = _assoc_list->get_state(sta);
  if  (BRN2AssocList::ASSOCIATED == state
    && !_id->isIdentical(&bssid))
  { // The previously associated sta moved to bssid
    // TODO check if bssid is a BRN node
    _sta_tracker->sta_roamed(sta, bssid, *_id->getMasterAddress());
  }

  _assoc_list->client_seen(sta, bssid, dev);

  // Generate iapp hello message, if not already known
  BRN2AssocList::client_state new_state = _assoc_list->get_state(sta);
  if (BRN2AssocList::SEEN_OTHER == new_state
    &&new_state != state)
  {
    // Do not send immediatly, because otherwise route requests will always collide
    // with hello messages.
    _hello_handler->schedule_hello(sta);
  }
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG, H_OPTIMIZE};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappStationSnooper *td = (BrnIappStationSnooper *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_OPTIMIZE:
    return String(td->_optimize) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BrnIappStationSnooper *f = (BrnIappStationSnooper *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: 
    {    //debug
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
  case H_OPTIMIZE:
    {    //debug
      bool optimize;
      if (!cp_bool(s, &optimize)) 
        return errh->error("optimize parameter must be boolean");
      f->set_optimize(optimize);
      break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationSnooper::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("optimize", read_param, (void *) H_OPTIMIZE);
  add_write_handler("optimize", write_param, (void *) H_OPTIMIZE);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappStationSnooper)

////////////////////////////////////////////////////////////////////////////////
