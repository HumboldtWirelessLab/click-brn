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
 * brniapprouteupdatehandler.{cc,hh} -- handles the inter-ap protocol within brn
 * M. Kurth
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "elements/brn/routing/dsr/brn2_routequerier.hh"

#include "brniapprouteupdatehandler.hh"
#include "brniappencap.hh"
#include "brniapphellohandler.hh"
#include "brniapprotocol.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappRouteUpdateHandler::BrnIappRouteUpdateHandler() :
  _debug(BrnLogger::DEFAULT),
  _id(NULL),
  _encap(NULL),
  _hello_handler(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappRouteUpdateHandler::~BrnIappRouteUpdateHandler()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappRouteUpdateHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"AssocList element",*/ &_assoc_list,
      "ENCAP", cpkP+cpkM, cpElement, /*"BrnIapp encap element",*/ &_encap,
      "HELLOHDL", cpkP+cpkM, cpElement, /*"HelloHandler element",*/ &_hello_handler,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_id,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      cpEnd) < 0)
    return -1;

  if (!_assoc_list || !_assoc_list->cast("BRN2AssocList")) 
    return errh->error("BRN2AssocList not specified");

  if (!_encap || !_encap->cast("BrnIappEncap")) 
    return errh->error("BrnIappEncap not specified");

  if (!_hello_handler || !_hello_handler->cast("BrnIappHelloHandler")) 
    return errh->error("HelloHandler not specified");

  if (!_id || !_id->cast("BRN2NodeIdentity"))
    return errh->error("BRN2NodeIdentity not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappRouteUpdateHandler::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappRouteUpdateHandler::push(int, Packet *p)
{
  recv_handover_routeupdate(p);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappRouteUpdateHandler::send_handover_routeupdate(
  EtherAddress dst,
  EtherAddress src,
  EtherAddress sta,
  EtherAddress ap_new, 
  EtherAddress ap_old,
  uint8_t      seq_no)
{
  BRN_CHECK_EXPR_RETURN(dst == src || *_id->getMasterAddress() == dst,
    ("called with invalid arguments: src %s, dst %s, sta %s, new %s, old %s", 
    src.unparse().c_str(), dst.unparse().c_str(), sta.unparse().c_str(), 
    ap_new.unparse().c_str(), ap_old.unparse().c_str()), return);

  BRN_DEBUG("send route update from %s to %s (sta %s, new %s, old %s)", 
    src.unparse().c_str(), dst.unparse().c_str(), sta.unparse().c_str(), 
    ap_new.unparse().c_str(), ap_old.unparse().c_str());


  // push out
  Packet* p = _encap->create_handover_routeupdate(
    dst, src, sta, ap_new, ap_old, seq_no);
  output(0).push(p);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappRouteUpdateHandler::recv_handover_routeupdate(
  Packet* p)
{
  BRN_CHECK_EXPR_RETURN(p == NULL || p->length() < sizeof(struct click_brn_iapp),
    ("invalid argument"), if (p) p->kill(); return;);

  // Note: the link table update is done in peek()
  click_brn_iapp*     pIapp   = (click_brn_iapp*)p->data();
  click_brn_iapp_ho*  pHo     = &pIapp->payload.ho;

  BRN_CHECK_EXPR_RETURN(CLICK_BRN_IAPP_HRU != pIapp->type,
    ("got invalid iapp type %d", pIapp->type), if (p) p->kill(); return;);

  EtherAddress sta   (pHo->addr_sta);
  EtherAddress ap_old(pHo->addr_mold);
  EtherAddress ap_new(pHo->addr_mnew);
  uint8_t      seq_no(pHo->seq_no);

  BRN_DEBUG("received route update for sta %s, new %s and old %s", 
    sta.unparse().c_str(), ap_new.unparse().c_str(), ap_old.unparse().c_str());

  // Check if the link update really proceeded
  if (_debug >= BrnLogger::INFO) {

    BRN_CHECK_EXPR(BRN_DSR_STATION_METRIC < _link_table->get_link_metric(ap_new, sta) 
      || BRN_DSR_STATION_METRIC < _link_table->get_link_metric(sta, ap_new),
        ("corrupted link table, missing link from sta %s to new ap %s",
          sta.unparse().c_str(), ap_new.unparse().c_str()));

    BRN_CHECK_EXPR(BRN_DSR_ROAMED_STATION_METRIC > _link_table->get_link_metric(ap_old, sta) 
      || BRN_DSR_INVALID_ROUTE_METRIC > _link_table->get_link_metric(sta, ap_old),
        ("corrupted link table, link from sta %s to old ap %s still exists",
          sta.unparse().c_str(), ap_old.unparse().c_str()));
  }

  // get the ether header
  click_ether* ether = (click_ether*) p->ether_header();
  BRN_CHECK_EXPR_RETURN(NULL == ether, 
    ("missing ether header"), p->kill();return;);

  /// @TODO eleminate duplicates by using seq_no and ap_new
  UNREFERENCED_PARAMETER(seq_no);

  // Finally kill the packet
  p->kill();

  // Check if there is a route to the new ap, otherwise initiate a route discovery
  // We simply use a hello here for this purpose, because it is to_curr no one
  // will learn from this packet
  _hello_handler->send_iapp_hello(sta, *_id->getMasterAddress(), ap_new, true);
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappRouteUpdateHandler *td = (BrnIappRouteUpdateHandler *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BrnIappRouteUpdateHandler *f = (BrnIappRouteUpdateHandler *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappRouteUpdateHandler::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappRouteUpdateHandler)


////////////////////////////////////////////////////////////////////////////////
