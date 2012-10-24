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
 * brniappstationtracker.{cc,hh} -- keeps track of associated stations
 * M. Kurth
 */

#include <click/config.h>
#include <clicknet/wifi.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/wifi/ap/brn2_brnassocresponder.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/brn2.h"

#include "elements/brn/routing/dsr/brn2_dsrprotocol.hh"

#include "brniapprotocol.hh"
#include "brniappnotifyhandler.hh"
#include "brniappdatahandler.hh"
#include "brniappstationtracker.hh"
#include "signal.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappStationTracker::BrnIappStationTracker() :
  _debug(BrnLogger::DEFAULT),
  _optimize(true),
  _seq_no(0),
  _timer(static_timer_clear_stale, this),
  _id(NULL),
  _assoc_list(NULL),
  _link_table(NULL),
  _notify_handler(NULL),
  _data_handler(NULL),
  _assoc_responder(NULL)
  //_sig_assoc(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappStationTracker::~BrnIappStationTracker()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappStationTracker::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int stale_period = 120;
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP+cpkM, cpInteger, &_debug, /*"Debug"*/
      "STALE", cpkP+cpkM, cpUnsigned, &stale_period, /*"Stale info timeout"*/
      "OPTIMIZE", cpkP+cpkM, cpBool, &_optimize, /*"Optimize"*/
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"AssocList element",*/ &_assoc_list,
      "NOTIFYHDL", cpkP+cpkM, cpElement, /*"NotifyHandler element",*/ &_notify_handler,
      "DATAHDL", cpkP+cpkM, cpElement,/* "DataHandler element",*/ &_data_handler,
      "ASSOC_RESP", cpkP+cpkM, cpElement, /*"AssocResponder element",*/ &_assoc_responder,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_id,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      //"SIG_ASSOC", cpElement, "SignalAssoc element", &_sig_assoc,
      cpEnd) < 0)
    return -1;

  if (!_assoc_list || !_assoc_list->cast("AssocList")) 
    return errh->error("AssocList not specified");

  if (!_notify_handler || !_notify_handler->cast("BrnIappNotifyHandler")) 
    return errh->error("NotifyHandler not specified");

  if (!_data_handler || !_data_handler->cast("BrnIappDataHandler")) 
    return errh->error("DataHandler not specified");

  if (!_assoc_responder || !_assoc_responder->cast("BRNAssocResponder")) 
    return errh->error("AssocResponder not specified");

  /*
  if (!_sig_assoc || !_sig_assoc->cast("Signal") 
      || !_sig_assoc->is_signal("send_gratious_arp")) 
    return errh->error("SignalAssoc not specified");
   */

  _stale_timeout.tv_sec = stale_period;
  _stale_timeout.tv_usec = 0;

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappStationTracker::initialize(ErrorHandler *errh)
{
  if (!_id || !_id->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_link_table || !_link_table->cast("Brn2LinkTable")) 
    return errh->error("BRN2LinkTable not specified");

  timeval lt_timeout;
  _link_table->get_stale_timeout(lt_timeout);
  if ( Timestamp(_stale_timeout) >= Timestamp(lt_timeout))
    BRN_WARN("Link table timeout less than station timeout, "
      "routes to clients will become inconsistent");

  _timer.initialize(this);
  _timer.schedule_after_msec(_stale_timeout.tv_sec * 1000);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappStationTracker::static_timer_clear_stale(Timer *t, void *v)
{
  BrnIappStationTracker *as = (BrnIappStationTracker *)v;
  as->clear_stale();
  t->schedule_after_msec(5000);
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappStationTracker::clear_stale() 
{
//  BRN_DEBUG("removing stale entries.");

  for (BRN2AssocList::iterator i = _assoc_list->begin(); i.live(); i++)
  {
    EtherAddress e = i.key();
    if ((unsigned) _stale_timeout.tv_sec < i.value().age())
    {
      // Update link table
      update_linktable(e, EtherAddress(), *_id->getMasterAddress());

      // remove from assoclist and linktable
      BRN_INFO("client %s timed out, removed.", e.unparse().c_str());
      _assoc_list->remove(e);

      // send disassoc with reason expired
      _assoc_responder->send_disassociation(e, WIFI_REASON_ASSOC_EXPIRE);
    }
  }

  // only when clients are around ...
  /*
  if (_assoc_list->begin().live())
    _sig_assoc->send_signal_action();
  */
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationTracker::push(int, Packet* p)
{
  BRN_CHECK_EXPR_RETURN(NULL == p || p->length() < sizeof(struct click_wifi),
    ("invalid arguments"), if (p) p->kill();return;);

  click_brn_iapp*     pIapp   = (click_brn_iapp*)p->data();
  if (CLICK_BRN_IAPP_HEL == pIapp->type)
  {
    if (!_optimize)
    {
      BRN_ERROR("optimization turned off, there should be no hellos.");
    }

    click_brn_iapp_he*  pHe     = &pIapp->payload.he;

    EtherAddress sta(pHe->addr_sta);
    EtherAddress ap_curr(pHe->addr_ap_curr);
    EtherAddress ap_cand(pHe->addr_ap_cand);
    uint8_t authoritive = pHe->authoritive;

    p->kill();

    if (!authoritive)
      return;

    BRN_INFO("peeking hello curr %s cand %s (client %s)", 
      ap_curr.unparse().c_str(), ap_cand.unparse().c_str(), sta.unparse().c_str());

    // Update link table
    update_linktable(sta, ap_curr);

    // learn from authoritive hello message, if we do not know it better ...
    BRN2AssocList::client_state state = _assoc_list->get_state(sta);
    if (BRN2AssocList::SEEN_OTHER == state
        || BRN2AssocList::SEEN_BRN == state)
    {
      _assoc_list->set_state(sta, BRN2AssocList::SEEN_BRN);
      _assoc_list->set_ap(sta, ap_curr);
    }
  }
  else
  {
    click_brn_iapp_ho*  pHo     = &pIapp->payload.ho;

    EtherAddress client(pHo->addr_sta);
    EtherAddress apOld(pHo->addr_mold);
    EtherAddress apNew(pHo->addr_mnew);

    if (!_optimize)
    {
      if (!_id->isIdentical(&apOld) 
        &&!_id->isIdentical(&apNew))
      {
        BRN_INFO("optimization turned off, not peeking packet.");
        p->kill();
        return;
      }
    }

    BRN_INFO("peeking packet type %d (new %s, old %s, sta %s)", 
      pIapp->type, apNew.unparse().c_str(), apOld.unparse().c_str(), client.unparse().c_str());

    p->kill();

    // Update link table
    update_linktable(client, apNew, apOld);

    // Check if the link update really proceeded
    if (_debug >= BrnLogger::INFO) {
      BRN_CHECK_EXPR(BRN_DSR_STATION_METRIC < _link_table->get_link_metric(apNew, client) 
        || BRN_DSR_STATION_METRIC < _link_table->get_link_metric(client, apNew),
        ("corrupted link table, missing link from sta %s to new ap %s",
          client.unparse().c_str(), apNew.unparse().c_str()));

      BRN_CHECK_EXPR(BRN_DSR_ROAMED_STATION_METRIC > _link_table->get_link_metric(apOld, client) 
        || BRN_DSR_INVALID_ROUTE_METRIC > _link_table->get_link_metric(client, apOld),
        ("corrupted link table, link from sta %s to old ap %s still exists",
          client.unparse().c_str(), apOld.unparse().c_str()));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationTracker::sta_associated(
  EtherAddress  sta,
  EtherAddress  ap_new,
  EtherAddress  ap_old,
  BRN2Device *device,
  const String& ssid)
{
  BRN_CHECK_EXPR_RETURN(!sta || !ap_new,
    ("invalid arguments"), return;);

  // Get the current state
  BRN2AssocList::client_state state = _assoc_list->get_state(sta);

  // Sanity check  
  BRN_CHECK_EXPR(_debug && ap_old && BRN2AssocList::NON_EXIST == state,
    ("sta %s reassoc'd but not known in advance. Promisc forgotten?", 
      sta.unparse().c_str()));

  // Check the old ap, if not set (like in the assoc case), lookup in own data...
  EtherAddress stored_ap_old = _assoc_list->get_ap(sta);
  if (!ap_old && 
    (BRN2AssocList::SEEN_BRN == state
       || BRN2AssocList::ROAMED == state
       || BRN2AssocList::ASSOCIATED == state))
  {
    // nugget: handling of hard handoff in the assoc case
    // note: improvement but no 100% solution
    ap_old = stored_ap_old;
  }

  // Insert into and update, if it is for us
  if (_id->isIdentical(&ap_new)) 
  {
    BRN_INFO("sta %s associated with %s on dev %s (old %s)", 
             sta.unparse().c_str(), ap_new.unparse().c_str(), device->getDeviceName().c_str(), ap_old.unparse().c_str());

    // cleanse linktable, remove all links to/from the associating station
    _link_table->remove_node(sta);

    // Update link table
    update_linktable(sta, ap_new, ap_old); 

    // insert the new information, invalidates all old data
    String my_ssid = (ssid == "" ? _assoc_list->get_ssid(sta) : ssid);
    _assoc_list->insert(sta, device, my_ssid, ap_old, _seq_no);
    _seq_no = (_seq_no + 1) % MAX_SEQ_NO;

    BRN2AssocList::ClientInfo* pClient = _assoc_list->get_entry(sta);
    BRN_CHECK_EXPR_RETURN(NULL == pClient,
      ("unable to gather assoc list entry"), return;);

    // If not associated or previously associated or the previous ap not known, ignore...
    if (BRN2AssocList::ASSOCIATED != pClient->get_state()
      || BRN2AssocList::ASSOCIATED == state
      || !pClient->get_old_ap()
      || pClient->get_ap() == pClient->get_old_ap())
      return;

    // Send the notify
    _notify_handler->send_handover_notify(sta, ap_new, ap_old, pClient->get_seq_no());

    // Send another notify if announced ap and stored ap are not the same
    // (could happen if the client announces the wrong one) 
    if (stored_ap_old 
      && stored_ap_old != ap_old)
      _notify_handler->send_handover_notify(sta, ap_new, stored_ap_old, 
        pClient->get_seq_no());
  }
  // The station associated with another mesh node
  else if (_id->isIdentical(&ap_old) 
    && ap_old != ap_new) 
  {
    sta_roamed(sta, ap_new, ap_old);
  }
  else
  {
    // Update link table
    update_linktable(sta, ap_new, ap_old); 

    _assoc_list->client_seen(sta, ap_new, device);
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationTracker::sta_disassociated(
  EtherAddress  sta)
{
  BRN_INFO("station %s disassociated", sta.unparse().c_str());

  // Update link table
  update_linktable(sta, EtherAddress(), *_id->getMasterAddress());

  // we don't care about whether the remove is sucessfull or not because
  // some client stations send multiple disassocs
  _assoc_list->disassociated(sta);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationTracker::sta_roamed(
  EtherAddress client, 
  EtherAddress apNew, 
  EtherAddress apOld )
{
  BRN_INFO("station %s roamed from %s to %s", 
    client.unparse().c_str(), apNew.unparse().c_str(), apOld.unparse().c_str());

  // Update link table
  update_linktable(client, apNew, apOld); 

  // If the final destination is reached, clean the internals and send reply
  if (_id->isIdentical(&apOld))
  {
    // Note: the bufferd packet list is cleared in the following roamed()!
    BRN2AssocList::PacketList pl = _assoc_list->get_buffered_packets(client);

    // Mark as roamed in assoc list
    // TODO put in correct sequence number
    _assoc_list->roamed(client, apNew, apOld, SEQ_NO_INF);

    BRN_INFO("salvaged %d packets for sta %s during handover", 
      pl.size(), client.unparse().c_str());

    // Send buffered packets to the new mesh node
    for (;0 < pl.size(); pl.pop_front()) {
      _data_handler->handle_handover_data(client, apNew, apOld, SEQ_NO_INF, pl.front());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

bool 
BrnIappStationTracker::update(EtherAddress sta)
{
  // An error is generated if not associated
  if (!_assoc_list->update(sta))
    return (false);

  // TODO improve performance
  EtherAddress ap(_assoc_list->get_ap(sta));
  return (ap && update_linktable(sta, ap));
}

////////////////////////////////////////////////////////////////////////////////

Packet* 
BrnIappStationTracker::filter_buffered_packet(
  Packet *p)
{
  if (!_optimize)
  {
    BRN_INFO("optimization turned off, not filtering packet.");
    return (p);
  }

  click_ether *ether = (click_ether *)p->ether_header();
  BRN_CHECK_EXPR_RETURN(!ether,
    ("ether anno not available"), return (p););

  EtherAddress src_addr(ether->ether_shost);
  BRN2AssocList::ClientInfo* pClient = _assoc_list->get_entry(src_addr);

  // If the packet does not come from an associated station or 
  // we do not know the former ap, exit
  if (NULL == pClient
    || BRN2AssocList::ASSOCIATED != pClient->get_state()
    || !pClient->get_old_ap()
    || pClient->get_old_ap() == pClient->get_ap())
    return (p);

  EtherAddress sta_ap(pClient->get_ap());
  BRN_CHECK_EXPR(!_id->isIdentical(&sta_ap),
    ("sta %s has ap %s, which is different from me", 
    src_addr.unparse().c_str(), pClient->get_ap().unparse().c_str()));

  BRN_INFO("filtering buffered packet for STA %s and send it to old ap %s", 
    pClient->get_eth().unparse().c_str(), pClient->get_old_ap().unparse().c_str());

  // Otherwise send the packet to the former ap instead of delaying it
  _data_handler->handle_handover_data(pClient->get_eth(),
                                      pClient->get_ap(),
                                      pClient->get_old_ap(),
                                      pClient->get_seq_no(),
                                      p); 
  return (NULL);
}

////////////////////////////////////////////////////////////////////////////////

bool 
BrnIappStationTracker::update_linktable(
  EtherAddress sta, 
  EtherAddress ap_new,
  EtherAddress ap_old ) 
{
  // Do not put non-brn nodes into the link table
  // TODO does not work will, to restrictive
//  AssocList::client_state state = _assoc_list->get_state(sta);
//  if (AssocList::NON_EXIST == state
//    ||AssocList::SEEN_OTHER == state)
//    return (false);

  // cleanse linktable, remove all links to/from the associating station
  // TODO improve performance, do not run it every time
  // Problem: if a stale entry remains in the link table, the routing constructs
  // invalid routes (with station as relay node)
  _link_table->remove_node(sta);

  // Update link table
  // TODO is 100 and 5000 ok?
  // TODO every time we update here the internal time stamp of each route entry
  // is updated. could this turned off in this case?
  bool ret = true;

  if (ap_new) {
    // TODO prevent loops and relaying of stations (inspect ap_old?)
    ret &= _link_table->update_link(sta, ap_new, 0, 0, BRN_DSR_STATION_METRIC, LINK_UPDATE_REMOTE);
    if (ret)
      BRN_DEBUG("_link_table->update_link %s %s %d\n",
        sta.unparse().c_str(), ap_new.unparse().c_str(), BRN_DSR_INVALID_ROUTE_METRIC);

    ret &= _link_table->update_link(ap_new, sta, 0, 0, BRN_DSR_STATION_METRIC, LINK_UPDATE_REMOTE);
    if (ret)
      BRN_DEBUG("_link_table->update_link %s %s %d\n",
        ap_new.unparse().c_str(), sta.unparse().c_str(), BRN_DSR_STATION_METRIC, LINK_UPDATE_REMOTE);
  }

  if (ap_old) {
    ret &= _link_table->update_link(sta, ap_old, 0, 0, BRN_DSR_INVALID_ROUTE_METRIC, LINK_UPDATE_REMOTE);
    if (ret)
      BRN_DEBUG("_link_table->update_link %s %s %d\n",
        sta.unparse().c_str(), ap_old.unparse().c_str(), BRN_DSR_INVALID_ROUTE_METRIC);

    // if not optimizing or the sta has no new ap, discart from route table
    int linkmetric_old_to_sta = BRN_DSR_ROAMED_STATION_METRIC;
    if (!_optimize || !ap_new)
    {
      BRN_DEBUG("optimization turned off, invalidating link old ap -> sta.");
      linkmetric_old_to_sta = BRN_DSR_INVALID_ROUTE_METRIC;
    }

    ret &= _link_table->update_link(ap_old, sta, 0, 0, linkmetric_old_to_sta, LINK_UPDATE_REMOTE);
    if (ret)
      BRN_DEBUG("_link_table->update_link %s %s %d\n",
        ap_old.unparse().c_str(), sta.unparse().c_str(), linkmetric_old_to_sta);
  }

  return (ret);

//  bool ret = _link_table->update_link(sta, ap_old, 0, 0, BRN_DSR_ROAMED_STATION_METRIC);
//  if (ret)
//    BRN_DEBUG("_link_table->update_link %s %s %d\n",
//      sta.unparse().c_str(), ap_old.unparse().c_str(), BRN_DSR_ROAMED_STATION_METRIC);
//
//  ret = _link_table->update_link(ap_old, sta, 0, 0, BRN_DSR_INVALID_ROUTE_METRIC);
//  if (ret)
//    BRN_DEBUG("_link_table->update_link %s %s %d\n",
//      ap_old.unparse().c_str(), sta.unparse().c_str(), BRN_DSR_INVALID_ROUTE_METRIC);
//
//  ret = _link_table->update_link(sta, ap_new, 0, 0, BRN_DSR_STATION_METRIC);
//  if (ret)
//    BRN_DEBUG("_link_table->update_link %s %s %d\n",
//      sta.unparse().c_str(), ap_new.unparse().c_str(), BRN_DSR_STATION_METRIC);
//
//  ret = _link_table->update_link(ap_new, sta, 0, 0, BRN_DSR_INVALID_ROUTE_METRIC);
//  if (ret)
//    BRN_DEBUG("_link_table->update_link %s %s %d\n",
//      ap_new.unparse().c_str(), sta.unparse().c_str(), BRN_DSR_INVALID_ROUTE_METRIC);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationTracker::disassoc_all(int reason)
{
  for (BRN2AssocList::iterator i = _assoc_list->begin(); i.live(); i++)
  {
    EtherAddress e = i.key();
    if (BRN2AssocList::ASSOCIATED == i.value().get_state())
    {
      // Update link table
      update_linktable(e, EtherAddress(), *_id->getMasterAddress());

      // remove from assoclist and linktable
      BRN_INFO("client %s disassociated", e.unparse().c_str());
      _assoc_list->remove(e);

      // send disassoc with reason expired
      _assoc_responder->send_disassociation(e, reason);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG, H_OPTIMIZE, H_DISASSOC};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappStationTracker *td = (BrnIappStationTracker *)e;
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
  BrnIappStationTracker *f = (BrnIappStationTracker *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
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
        return errh->error("optimize parameter must be int");
      f->_optimize = optimize;
      break;
    }
  case H_DISASSOC:
    {
      int reason;
      if (!cp_integer(s, &reason)) 
        return errh->error("reason parameter must be int");
      f->disassoc_all(reason);
      break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappStationTracker::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("optimize", read_param, (void *) H_OPTIMIZE);
  add_write_handler("optimize", write_param, (void *) H_OPTIMIZE);

  add_write_handler("disassoc", write_param, (void *) H_DISASSOC);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappStationTracker)

////////////////////////////////////////////////////////////////////////////////
