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
 * brniappdatahandler.{cc,hh} -- handles the inter-ap protocol type data within brn
 * M. Kurth
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brniapprotocol.hh"
#include "brniappdatahandler.hh"
#include "brniappencap.hh"
#include "brniapprouteupdatehandler.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappDataHandler::BrnIappDataHandler() :
  _debug(BrnLogger::DEFAULT),
  _optimize(true),
  _id(NULL),
  _assoc_list(NULL),
  _encap(NULL),
  _route_handler(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappDataHandler::~BrnIappDataHandler()
{
}

////////////////////////////////////////////////////////////////////////////////
int 
BrnIappDataHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"AssocList element",*/ &_assoc_list,
      "ENCAP", cpkP+cpkM, cpElement, /*"BrnIapp encap element",*/ &_encap,
      "ROUTEHDL", cpkP+cpkM, cpElement, /*"RouteUpdateHandler element",*/ &_route_handler,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_id,
      "OPTIMIZE", cpkP, cpBool, /*"Optimize",*/ &_optimize,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      cpEnd) < 0)
    return -1;

  if (!_assoc_list || !_assoc_list->cast("BRN2AssocList")) 
    return errh->error("AssocList not specified");

  if (!_encap || !_encap->cast("BrnIappEncap")) 
    return errh->error("BrnIappEncap not specified");

  if (!_route_handler || !_route_handler->cast("BrnIappRouteUpdateHandler")) 
    return errh->error("RouteUpdateHandler not specified");

  if (!_id || !_id->cast("BRN2NodeIdentity"))
    return errh->error("NodeIdentity not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappDataHandler::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappDataHandler::push(int port, Packet *p)
{
  if (0 == port)
  {
    recv_handover_data(p);
  }
  else //if (1 == port)
  {
    recv_ether(p);
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappDataHandler::recv_handover_data(
  Packet* p)
{
  BRN_CHECK_EXPR_RETURN(p == NULL || p->length() < sizeof(struct click_brn_iapp),
    ("invalid argument (recv_handover_data)"), if (p) p->kill(); return;);

  click_brn_iapp*     pIapp   = (click_brn_iapp*)p->data();
  click_brn_iapp_ho*  pHo     = &pIapp->payload.ho;

  BRN_CHECK_EXPR_RETURN(CLICK_BRN_IAPP_DAT != pIapp->type,
    ("got invalid iapp type %d", pIapp->type), if (p) p->kill(); return;);

  // Strip iapp header, now p should be ethernet data
  p->pull(sizeof(click_brn_iapp));
  BRN_CHECK_EXPR_RETURN(p->length() < sizeof(struct click_ether),
    ("packet too small: %d vs %d", p->length(), sizeof(struct click_ether)), 
    if (p) p->kill(); return;);

  uint16_t len = ntohs(pIapp->payload_len);
  BRN_CHECK_EXPR_RETURN(len != p->length(),
    ("payload length differ (%d != %d)", len, p->length()), 
    if (p) p->kill(); return;);

  // Get the ether header (NOTE needed for output(0).push(p))
  const click_ether* ether = (const click_ether*) p->data();
  p->set_ether_header(ether);
  BRN_CHECK_EXPR_RETURN(NULL == ether, 
    ("missing ether header"), p->kill();return;);

  // dst could be roamed station or corresponding node
  EtherAddress dst(ether->ether_dhost);
  EtherAddress src(ether->ether_shost);
  EtherAddress client(pHo->addr_sta);
  EtherAddress apOld(pHo->addr_mold);
  EtherAddress apNew(pHo->addr_mnew);

  BRN_DEBUG("received data sta %s new %s old %s (src %s -> dst %s)", 
    client.unparse().c_str(), apNew.unparse().c_str(), apOld.unparse().c_str(),
    src.unparse().c_str(), dst.unparse().c_str());

  // case 1: packet is for the current node or associated client
  if (_id->isIdentical(&dst) || _assoc_list->is_associated(dst))
  {
    // Give the packet to output(0) and to [0]dsr
    output(0).push(p);
    return;
  }

  // if not case 1, check if roamed: if neighter roamed nor assoc'd, discard
  BRN2AssocList::ClientInfo* pClient = _assoc_list->get_entry(client);
  BRN_CHECK_EXPR_RETURN(NULL == pClient || BRN2AssocList::ROAMED != pClient->get_state(),
    ("station %s unknown or in unexpected state %d", dst.unparse().c_str(),
      pClient ? pClient->get_state() : -1), p->kill(); return;);

  // Sta roamed, check in which direction the packet should flow...
  if (client == dst) // cn -> sta
  {
    // let handle_ether do the thing...
    recv_ether(p); 
    return;
  }
  else if (client == src) // sta -> cn
  {
    EtherAddress cn(ether->ether_dhost);
    BRN_DEBUG("forwarding packet from sta %s to cn %s.", 
      pClient->get_eth().unparse().c_str(), cn.unparse().c_str());

    // send the data to the cn
    send_handover_data( cn,
                        *_id->getMasterAddress(),
                        pClient->get_eth(),
                        pClient->get_ap(),
                        pClient->get_old_ap(),
                        p);
    return;
  }

  // unknown direction, discard
  BRN_ERROR("unable to handle received iapp data.");
  p->kill();
  return;
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappDataHandler::recv_ether(Packet* p)
{
  BRN_CHECK_EXPR_RETURN(p == NULL || p->length() < sizeof(struct click_ether),
    ("invalid argument (recv_ether, 0x%x, len %d)", p, p ? p->length() : 0), 
    if (p) p->kill(); return;);

  // Get ether header
  const click_ether* ether = (const click_ether*) p->data();
  p->set_ether_header(ether);
  BRN_CHECK_EXPR_RETURN(NULL == ether, ("missing ether header"), p->kill();return;);

  // Check the dst address
  EtherAddress ether_dst(ether->ether_dhost);
  BRN2AssocList::ClientInfo* pClient = _assoc_list->get_entry(ether_dst);
  if (NULL == pClient)
  {
    BRN_INFO("node %s is not a known sta, discard ether packet.", 
      ether_dst.unparse().c_str());
    p->kill();
    return;
  }

  // Check if it is our client and we should buffer the packet
  BRN2AssocList::client_state state = pClient->get_state();
  if (BRN2AssocList::ASSOCIATED == state) {
    if (!_optimize)
    {
      BRN_DEBUG("optimization turned off, killing packet.");
      p->kill();
      return;
    }

    BRN_DEBUG("buffering not delivered packet for STA %s (roamed?).", 
      ether_dst.unparse().c_str());

    _assoc_list->buffer_packet(ether_dst, p); 
    p = NULL;
    return;
  }

  // If not in state roamed, kill the packet  
  if(BRN2AssocList::ROAMED != state)
  {
    // this could happen if we have seen the client and we are in the source 
    // route: if the packet does not make the hop to the next node, we get
    // this message here.
    BRN_INFO("killing packet for STA %s neighter assoc'd nor roaming.", 
      ether_dst.unparse().c_str());
    p->kill();
    return;
  }

  if (!_optimize)
  {
    BRN_DEBUG("optimization turned off, not forwarding packet to new ap.");
    p->kill();
    return;
  }

  EtherAddress ap_new = _assoc_list->get_ap(ether_dst);
  BRN_CHECK_EXPR_RETURN(!ap_new,
    ("could not determine new ap for STA %s.", ether_dst.unparse().c_str()),
    p->kill();return;);

  BRN_DEBUG("forwarding packet for STA %s to %s.",
    pClient->get_eth().unparse().c_str(), pClient->get_ap().unparse().c_str());

  handle_handover_data( pClient->get_eth(),
                        pClient->get_ap(),
                        pClient->get_old_ap(),
                        pClient->get_seq_no(),
                        p);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappDataHandler::handle_handover_data(
  EtherAddress sta, 
  EtherAddress ap_new, 
  EtherAddress ap_old,
  uint8_t      seq_no,
  Packet*      p )
{
  BRN_CHECK_EXPR_RETURN(!p || !sta || !ap_new || !ap_old,
    ("invalid arguments"), if (p) p->kill();return;);

  // Get the ether header
  const click_ether *ether = (const click_ether *)p->ether_header();
  BRN_CHECK_EXPR_RETURN(NULL == ether,
    ("missing ether header"), p->kill();return;);

  EtherAddress dst(ap_old); // assume sending packet to old ap
  EtherAddress src(*_id->getMasterAddress());

  // Check for destination cn -> sta (send packet to new ap)
  if (EtherAddress(ether->ether_dhost) == sta) 
  {
    // Send the data packet to the new ap
    dst = ap_new; 

    // Source is corresponding node
    EtherAddress cn(ether->ether_shost);  

    BRN2AssocList::ClientInfo* pClient = _assoc_list->get_entry(sta);
    BRN_CHECK_EXPR(NULL == pClient,
      ("client %s not found", sta.unparse().c_str()));

    // Send handover route update, only once per cn
    if (pClient && !pClient->contains_cn(cn) && cn != src)
    {
      pClient->add_cn(cn);
      if (!_optimize)
      {
        BRN_DEBUG("optimization turned off, generating route error");
        Packet* p2 = p->clone();
 //       p2->set_dst_ether_anno(EtherAddress(ether->ether_dhost));
        output(1).push(p2);
      }
      else
      {
        _route_handler->send_handover_routeupdate(cn, src, sta, ap_new, ap_old, seq_no);
      }
    }
  }
  else
  {
    // destination sta -> cn (send packet to old ap)
    if (!_optimize)
    {
      BRN_DEBUG("optimization turned off, killing packet.");
      p->kill();
      return;
    }
  }

  // Send the data
  send_handover_data(dst, src, sta, ap_new, ap_old, p);
}

////////////////////////////////////////////////////////////////////////////////


void
BrnIappDataHandler::send_handover_data(
  EtherAddress dst, 
  EtherAddress src, 
  EtherAddress client, 
  EtherAddress apNew, 
  EtherAddress apOld,
  Packet*      p_in ) 
{
  BRN_DEBUG("send data from %s to %s (sta %s, new %s, old %s)", 
    src.unparse().c_str(), dst.unparse().c_str(), client.unparse().c_str(), 
    apNew.unparse().c_str(), apOld.unparse().c_str());

  p_in = _encap->create_handover_data(dst, src, client, apNew, apOld, p_in);

  /* 
   * Put ether packet into dsr[0] for re-routing, the linktable should contain
   * the new link to the station as given in the handover notify.
   */
  if (p_in)
    output(0).push(p_in);
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG, H_OPTIMIZE};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappDataHandler *td = (BrnIappDataHandler *)e;
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
  BrnIappDataHandler *f = (BrnIappDataHandler *)e;
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
        return errh->error("optimize parameter must be boolean");
      f->_optimize = optimize;
      break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappDataHandler::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("optimize", read_param, (void *) H_OPTIMIZE);
  add_write_handler("optimize", write_param, (void *) H_OPTIMIZE);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappDataHandler)

////////////////////////////////////////////////////////////////////////////////
