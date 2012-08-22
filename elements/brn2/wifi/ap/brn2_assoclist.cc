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
 * assoclist.{cc,hh} -- stores the ethernet address of associated node (clients, brn nodes)
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"

#include "brn2_assoclist.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BRN2AssocList::BRN2AssocList()
  : _debug(BrnLogger::DEFAULT),
    _client_qsize_max(20),
    _link_table(NULL)
{
  _client_list = new ClientMap();
}

////////////////////////////////////////////////////////////////////////////////

BRN2AssocList::~BRN2AssocList()
{
  BRN_DEBUG(print_content().c_str());

  delete _client_list;
}

////////////////////////////////////////////////////////////////////////////////

String
BRN2AssocList::print_content() const
{
  StringAccum sa;

  //sa << "assoc list(" << *_id->getMyWirelessAddress() << "):";

  for (ClientMap::iterator i = _client_list->begin(); i.live(); i++)
  {
    ClientInfo nfo = i.value();
    const char* state = " n/a ";
    switch (nfo.get_state())
    {
      case NON_EXIST:
        state = " nonexist ";
        break;
      case SEEN_OTHER:
        state = " other    ";
        break;
      case SEEN_BRN:
        state = " with_brn ";
        break;
      case ASSOCIATED:
        state = " assoc    ";
        break;
      case ROAMED:
        state = " roamed   ";
        break;
    }

    sa << "\n *) on SSID " << nfo.get_ssid() << " " << nfo.get_eth() << state /*<< nfo.get_dev_name() */<< " " 
      << nfo.get_ap().unparse() << " " << nfo.get_age() << " " << nfo.get_last_updated();
  }

  return sa.take_string();
}

////////////////////////////////////////////////////////////////////////////////

int
BRN2AssocList::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  ret = cp_va_kparse(conf, this, errh,
        "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
        "BUFFER", cpkP, cpUnsigned, /*"Buffer for pre-handover packets",*/ &_client_qsize_max,
        cpEnd);

  return ret;
}

////////////////////////////////////////////////////////////////////////////////

int
BRN2AssocList::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

/* checks if a node with the given ethernet address is associated with us */
bool
BRN2AssocList::is_associated(const EtherAddress& v) const
{
  ClientInfo* pClient = _client_list->findp(v);
  if (NULL == pClient)
    return false;
  
  return (ASSOCIATED == pClient->get_state());
}

////////////////////////////////////////////////////////////////////////////////

bool 
BRN2AssocList::is_roaming(const EtherAddress& v) const
{
  ClientInfo* pClient = _client_list->findp(v);
  if (NULL == pClient)
    return false;
  
  return (ROAMED == pClient->get_state());
}

////////////////////////////////////////////////////////////////////////////////

BRN2AssocList::ClientInfo *
BRN2AssocList::get_entry(const EtherAddress& v)
{
  return _client_list->findp(v);
}

////////////////////////////////////////////////////////////////////////////////

String
BRN2AssocList::get_device_name(const EtherAddress& v) const
{
  ClientInfo* pClient = _client_list->findp(v);
  //BRN_DEBUG(" _client_list->findp(*v) = 0x%x", pClient);

  if (NULL == pClient)
    return String();

  //BRN_DEBUG(" _client_list->findp(*v) = 0x%x", pClient);
/*  if (!pClient->get_dev_name()) 
  {
    BRN_WARN("missing device name. ethernet address = %s; device =" ,// %s ", 
             pClient->get_eth().unparse().c_str() ); //, pClient->get_dev_name().c_str());
  }
*/
 // return (pClient->get_dev_name());
  return "ath";
}

////////////////////////////////////////////////////////////////////////////////

String
BRN2AssocList::get_ssid(const EtherAddress& v) const
{
  ClientInfo* pClient = _client_list->findp(v);
  //BRN_DEBUG(" _client_list->findp(*v) = 0x%x", pClient);

  if (NULL == pClient)
    return String();

  //BRN_DEBUG(" _client_list->findp(*v) = 0x%x", pClient);
  if (!pClient->get_ssid()) 
  {
    BRN_WARN("missing SSID for ethernet address %s", 
             pClient->get_eth().unparse().c_str());
  }

  return (pClient->get_ssid());
}

////////////////////////////////////////////////////////////////////////////////

bool
BRN2AssocList::insert(  EtherAddress    eth,
                    BRN2Device      *dev,
                    String          ssid, 
                    EtherAddress    ap_old /*= EtherAddress()*/,
                    uint8_t         seq_no /*= SEQ_NO_INF*/)
{
  //BRN_DEBUG("Inserting address %s on device %s.", eth.unparse().c_str(), dev_name.c_str());

  // dirty assertion - jkm
//  BRN_CHECK_EXPR_RETURN(!eth || !dev_name,
  //                       ("You idiot, you tried to insert %s, %s", eth.unparse().c_str(), dev_name.c_str()),
 //   return false;);

  ClientInfo *client_info = _client_list->findp(eth);
  if (!client_info) 
  {
    _client_list->insert(eth, ClientInfo(NON_EXIST, eth, EtherAddress(dev->getEtherAddress()->data()), dev, ssid));
    client_info = _client_list->findp(eth);

 //   BRN_DEBUG("new entry added: %s, %s", eth.unparse().c_str(), dev_name.c_str());
  }

  // update entry
  client_info->update(0);
  client_info->set_state(ASSOCIATED);
  client_info->set_ap(EtherAddress(dev->getEtherAddress()->data()));
  client_info->set_old_ap(ap_old);
  client_info->set_seq_no(seq_no);
  client_info->set_ssid(ssid);

  _link_table->update_both_links(eth, EtherAddress(dev->getEtherAddress()->data()), 0, 0, 50, true);
  _link_table->associated_host(eth);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BRN2AssocList::roamed(
  EtherAddress    client,
  EtherAddress    ap_new,
  EtherAddress    ap_old,
  uint8_t         seq_nr )
{
  UNREFERENCED_PARAMETER(ap_old);
  
  ClientInfo* pClient = get_entry(client);
  
  BRN_CHECK_EXPR_RETURN(NULL == pClient,
                        ("client %s not known, not marked as roaming.", client.unparse().c_str()), return;);

  BRN_DEBUG("client %s roamed from %s to %s.", client.unparse().c_str(),
            ap_old.unparse().c_str(), ap_new.unparse().c_str());

  pClient->update(0);
  pClient->set_state(ROAMED);
  pClient->set_ap(ap_new);
  pClient->set_old_ap(ap_old);
  pClient->set_seq_no(seq_nr);
}

////////////////////////////////////////////////////////////////////////////////

void 
BRN2AssocList::disassociated(
  EtherAddress    client)
{
  assert (client);

  BRN_DEBUG("client %s disassociated.", client.unparse().c_str());

  _client_list->remove(client);
  
  _link_table->remove_node(client);
  
//  ClientInfo *client_info = _client_list->findp(client);
//  if (NULL == client_info)
//    return;
//
//  switch (client_info->get_state()) {
//  case NON_EXIST:
//  case SEEN_OTHER:
//    _client_list->remove(client);
//    return;
//  }
//
//  BRN_DEBUG("disassociation of STA %s reported.", client.unparse().c_str());
//
//  // the client becomes a zombie (state other, but no ap)
//  // It is removed when the entry times out, but if the sta reassociates we
//  // still have all information available.
//  if (client_info->get_ap())
//    client_info->set_old_ap(client_info->get_ap());
//  client_info->set_ap(EtherAddress());
//  client_info->set_state(SEEN_BRN);
//  client_info->update(0);
}

////////////////////////////////////////////////////////////////////////////////

bool 
BRN2AssocList::client_seen(
  EtherAddress    ether_sta, 
  EtherAddress    ether_bssid,
  BRN2Device      *dev)
{
  assert (ether_sta && ether_bssid);
  BRN_DEBUG("sighting of STA %s on BSSID %s reported.", 
            ether_sta.unparse().c_str(), ether_bssid.unparse().c_str());
  
  // Find entry in client list, create if necessary
  ClientInfo *client_info = _client_list->findp(ether_sta);
  bool found = (NULL != client_info);
  if (!found) 
  {
    _client_list->insert(ether_sta, ClientInfo(SEEN_OTHER, ether_sta, ether_bssid, dev, ""));
    client_info = _client_list->findp(ether_sta);

//    BRN_DEBUG("new entry added: %s, %s", ether_sta.unparse().c_str(), dev_name.c_str());
  }

  // Check switch in device, genereate a notice
//  if (dev_name != client_info->get_dev_name()) 
 // {
 //   BRN_DEBUG("client %s changed dev from %s to %s", ether_sta.unparse().c_str(),
//    client_info->get_dev_name().c_str(), dev_name.c_str());
//    client_info->set_dev_name(dev_name);
//  }

  // Check change in access point
  if (client_info->get_ap() != ether_bssid) 
  {
    BRN_DEBUG("client %s changed ap from %s to %s", ether_sta.unparse().c_str(),
              client_info->get_ap().unparse().c_str(), ether_bssid.unparse().c_str());

    // otherwise it would be a handoff which we could not handle here...
    assert (ASSOCIATED != client_info->get_state());
    if (ASSOCIATED != client_info->get_state())
      return (found);

    // Set state is seen other, because we must verify the association
    client_info->set_ap(ether_bssid);
    client_info->set_state(SEEN_OTHER);
    found = false;
  }
  
  // NOTE: invalidates _failed_q
  client_info->update(0);

  return (found);
}

////////////////////////////////////////////////////////////////////////////////

BRN2AssocList::client_state
BRN2AssocList::get_state(
  const EtherAddress&   v) const
{
  ClientInfo* pClient = _client_list->findp(v);
  
  return (NULL == pClient ? NON_EXIST : pClient->get_state());
}

////////////////////////////////////////////////////////////////////////////////

void 
BRN2AssocList::set_state(
  const EtherAddress&   v,
  client_state          new_state )
{
  ClientInfo* pClient = _client_list->findp(v);
  if (NULL == pClient)
    return;
    
  BRN_DEBUG("client %s changing state to %d.", v.unparse().c_str(), new_state);
  
  pClient->set_state(new_state);
}

////////////////////////////////////////////////////////////////////////////////

EtherAddress 
BRN2AssocList::get_ap(const EtherAddress& client) const
{
  ClientInfo* pClient = _client_list->findp(client);
  
  if (NULL == pClient) {
    BRN_ERROR("client %s not known (get_ap).", client.unparse().c_str());
    return EtherAddress();
  }
  
  return (pClient->get_ap());
}

////////////////////////////////////////////////////////////////////////////////

void 
BRN2AssocList::set_ap(
  const EtherAddress& sta,
  const EtherAddress& ap)
{
  ClientInfo* pClient = _client_list->findp(sta);
  
  if (NULL == pClient) {
    BRN_ERROR("client %s not known (set_ap).", sta.unparse().c_str());
    return;
  }
  
  // Could not handle handoff here, do it elsewhere ...
  assert (ASSOCIATED != pClient->get_state());
  pClient->set_ap(ap);
}

////////////////////////////////////////////////////////////////////////////////

void
BRN2AssocList::buffer_packet(
  EtherAddress ether_dst, 
  Packet* p)
{
  assert (ether_dst && p);
  ClientInfo* pClient = get_entry(ether_dst);
  
  if (NULL == pClient) {
    BRN_ERROR("client %s not known, packet killed.", ether_dst.unparse().c_str());
    p->kill();
    return;
  }
  
  pClient->get_failed_q().push_back(p);
  pClient->_clear_counter = 0;
  if (_client_qsize_max < pClient->get_failed_q().size()) {
    BRN_WARN("roaming buffer overflow for client %s (max size %d), killing packet.",
             ether_dst.unparse().c_str(), _client_qsize_max);

    pClient->get_failed_q().front()->kill();
    pClient->get_failed_q().pop_front();
  }
}
////////////////////////////////////////////////////////////////////////////////

BRN2AssocList::PacketList
BRN2AssocList::get_buffered_packets(
  EtherAddress ether_dst)
{
  assert (ether_dst);
  ClientInfo* pClient = get_entry(ether_dst);
  
  
  if (NULL == pClient) {
    BRN_ERROR("client %s not known (get_buffered_packets).", ether_dst.unparse().c_str());
    return PacketList();
  }
  
  if (ASSOCIATED != pClient->get_state()
      && ROAMED != pClient->get_state()) {
    BRN_ERROR("Unexpected state %u.", pClient->get_state());
    pClient->get_failed_q().clear();
    return PacketList(); 
  }

  return (pClient->release_failed_q());
}

////////////////////////////////////////////////////////////////////////////////

bool 
BRN2AssocList::update(EtherAddress sta)
{
  BRN_CHECK_EXPR_RETURN(!sta,
                         ("You idiot, you tried to update %s", sta.unparse().c_str()), return false;);

  ClientInfo* pClient = get_entry(sta);
  if (!pClient 
    ||ASSOCIATED != pClient->get_state())
  {
    BRN_DEBUG("Client %s not found or not associated", sta.unparse().c_str());
    return (false);
  }
  
  // update entry
  pClient->update(0);
  return (true);
}

////////////////////////////////////////////////////////////////////////////////

bool 
BRN2AssocList::remove(EtherAddress eth)
{
  if (NULL == _client_list->findp(eth))
    return (false);

/*  _link_table->update_both_links( *_dev->getEtherAddress(), eth,
    0, 0, BRN_DSR_INVALID_ROUTE_METRIC, false); 
*/
  return (_client_list->remove(eth));
}

////////////////////////////////////////////////////////////////////////////////

static String
read_debug_param(Element *e, void *thunk)
{
  UNREFERENCED_PARAMETER(thunk);

  BRN2AssocList *al = (BRN2AssocList *)e;
  return String(al->_debug) + "\n";
}

////////////////////////////////////////////////////////////////////////////////

static int 
write_debug_param(const String &in_s, Element *e, void *vparam,
  ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(vparam);

  BRN2AssocList *al = (BRN2AssocList *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  al->_debug = debug;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

static String
read_stations(Element *e, void *thunk)
{
  UNREFERENCED_PARAMETER(thunk);
  BRN2AssocList *al = (BRN2AssocList *)e;

  return (al->print_content());  
}

////////////////////////////////////////////////////////////////////////////////

/*
 * Adds a new entry into _assoc_list.
 * Format: insert $ETHERNET_ADDR $DEV_NAME
 */
static int
static_insert(const String &arg, Element */*e*/, void *p, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(p);

 // BRN2AssocList *f = (BRN2AssocList *)e;

  Vector<String> args;
  EtherAddress eth;
  String dev_name;
  String ssid;
  cp_spacevec(arg, args);

  if (!cp_ethernet_address(args[0], &eth)) {
    return errh->error("Couldn't read EthernetAddress out of arg");
  }

  if (!cp_string(args[1], &dev_name)) {
    return errh->error("Couldn't read device name out of arg");
  }
  
  if (!cp_string(args[1], &ssid)) {
    return errh->error("Couldn't read ssid out of arg");
  }
  
  click_chatter(" * inserting client node: %s from %s with SSID %s\n", eth.unparse().c_str(), dev_name.c_str(), ssid.c_str());
 /* if (!f->insert(eth, f->_dev, ssid))
  {
    errh->message("Node already listed!");
  }*/

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
BRN2AssocList::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_read_handler("stations", read_stations, 0);
  add_write_handler("debug", write_debug_param, 0);
  add_write_handler("insert", static_insert, 0);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2AssocList)

////////////////////////////////////////////////////////////////////////////////
