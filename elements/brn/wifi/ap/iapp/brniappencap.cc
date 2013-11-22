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
 * brniappencap.{cc,hh} -- encap for the inter-ap protocol within brn
 * M. Kurth
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>


#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/dsr/brn2_dsrprotocol.hh"

#include "brniapprotocol.hh"

#include "brniappencap.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappEncap::BrnIappEncap() :
  _debug(BrnLogger::DEFAULT)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappEncap::~BrnIappEncap()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappEncap::initialize(ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappEncap *td = (BrnIappEncap *)e;
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
  BrnIappEncap *f = (BrnIappEncap *)e;
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

void 
BrnIappEncap::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

Packet* 
BrnIappEncap::create_handover_notify(
  EtherAddress  client, 
  EtherAddress  apNew, 
  EtherAddress  apOld, 
  uint8_t       seq_no)
{
  BRN_DEBUG("Create notify sta %s new %s old %s seq %d", 
    client.unparse().c_str(), apNew.unparse().c_str(), apOld.unparse().c_str(), seq_no);

  // Create an empty iapp packet
  size_t size = sizeof(click_ether) + sizeof(click_brn) + sizeof(click_brn_iapp);
  WritablePacket* p = Packet::make(size);
  click_ether*        pEther = (click_ether*)p->data();
  click_brn*          pBrn   = (click_brn*)(pEther+1);
  click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
  click_brn_iapp_ho*  pHo     = &pIapp->payload.ho;

  // Fill iapp header
  pIapp->type         = CLICK_BRN_IAPP_HON;
  pIapp->payload_len  = 0;
  pHo->seq_no         = seq_no;
  memcpy(pHo->addr_sta,  client.data(), sizeof(pHo->addr_sta));
  memcpy(pHo->addr_mold, apOld.data(),  sizeof(pHo->addr_mold));
  memcpy(pHo->addr_mnew, apNew.data(),  sizeof(pHo->addr_mnew));
  
  // Fill brn header
  pBrn->dst_port      = BRN_PORT_IAPP;
  pBrn->src_port      = BRN_PORT_IAPP;
  pBrn->body_length   = htons(sizeof(click_brn_iapp));
  pBrn->ttl           = BRN_ROUTING_MAX_HOP_COUNT;
  pBrn->tos           = BRN_TOS_BE;
  
  // Fill ether header
  memcpy(pEther->ether_dhost, apOld.data(), sizeof(pEther->ether_dhost));
  memcpy(pEther->ether_shost, apNew.data(), sizeof(pEther->ether_shost));
  pEther->ether_type  = htons(ETHERTYPE_BRN); 
  p->set_ether_header(pEther);

  return (p);
}

////////////////////////////////////////////////////////////////////////////////

Packet* 
BrnIappEncap::create_handover_reply(
  EtherAddress client, 
  EtherAddress apNew, 
  EtherAddress apOld,
  uint8_t      seq_no )
{
  BRN_DEBUG("Create reply sta %s new %s old %s seq %d", 
    client.unparse().c_str(), apNew.unparse().c_str(), apOld.unparse().c_str(), seq_no);

  // Create an empty iapp packet
  size_t size = sizeof(click_ether) + sizeof(click_brn) + sizeof(click_brn_iapp);
  WritablePacket* p = Packet::make(size);
  click_ether*        pEther = (click_ether*)p->data();
  click_brn*          pBrn   = (click_brn*)(pEther+1);
  click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
  click_brn_iapp_ho*  pHo    = &pIapp->payload.ho;

  // Fill iapp header
  pIapp->type         = CLICK_BRN_IAPP_HOR;
  pIapp->payload_len  = 0;
  pHo->seq_no         = seq_no;
  memcpy(pHo->addr_sta,  client.data(), sizeof(pHo->addr_sta));
  memcpy(pHo->addr_mold, apOld.data(),  sizeof(pHo->addr_mold));
  memcpy(pHo->addr_mnew, apNew.data(),  sizeof(pHo->addr_mnew));
  
  // Fill brn header
  pBrn->dst_port      = BRN_PORT_IAPP;
  pBrn->src_port      = BRN_PORT_IAPP;
  pBrn->body_length   = htons(sizeof(click_brn_iapp));
  pBrn->ttl           = BRN_ROUTING_MAX_HOP_COUNT;
  pBrn->tos           = BRN_TOS_BE;
  
  // Fill ether header
  memcpy(pEther->ether_dhost, apNew.data(), sizeof(pEther->ether_dhost));
  memcpy(pEther->ether_shost, apOld.data(), sizeof(pEther->ether_shost));
  pEther->ether_type  = htons(ETHERTYPE_BRN); 
  p->set_ether_header(pEther);

  return (p);
}

////////////////////////////////////////////////////////////////////////////////

Packet* 
BrnIappEncap::create_handover_data(
  EtherAddress dst, 
  EtherAddress src, 
  EtherAddress client, 
  EtherAddress apNew, 
  EtherAddress apOld,
  Packet*      p_in )
{
  BRN_CHECK_EXPR_RETURN(!p_in || !client || !apNew || !apOld,
    ("invalid arguments"), if (p_in) p_in->kill();return (NULL););
  
  BRN_DEBUG("Create handover data src %s dst %s (sta %s, new %s, old %s)", 
    src.unparse().c_str(), dst.unparse().c_str(), client.unparse().c_str(), 
    apNew.unparse().c_str(), apOld.unparse().c_str());

  uint16_t payload_len = p_in->length();
  WritablePacket* p = p_in->push( sizeof(click_brn_iapp) + 
                                  sizeof(click_brn) + 
                                  sizeof(click_ether));
  
  click_ether*        pEther = (click_ether*)p->data();
  click_brn*          pBrn   = (click_brn*)(pEther+1);
  click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
  click_brn_iapp_ho*  pHo    = &pIapp->payload.ho;

  // Fill iapp header
  pIapp->type         = CLICK_BRN_IAPP_DAT;
  pIapp->payload_len  = htons(payload_len);
  memcpy(pHo->addr_sta,  client.data(), sizeof(pHo->addr_sta));
  memcpy(pHo->addr_mold, apOld.data(),  sizeof(pHo->addr_mold));
  memcpy(pHo->addr_mnew, apNew.data(),  sizeof(pHo->addr_mnew));
  
  // Fill brn header
  pBrn->dst_port      = BRN_PORT_IAPP;
  pBrn->src_port      = BRN_PORT_IAPP;
  pBrn->body_length   = htons(sizeof(click_brn_iapp) + payload_len);
  pBrn->ttl           = BRN_ROUTING_MAX_HOP_COUNT;
  pBrn->tos           = BRN_TOS_BE;
  
  // Fill ether header
  memcpy(pEther->ether_dhost, dst.data(), sizeof(pEther->ether_dhost));
  memcpy(pEther->ether_shost, src.data(), sizeof(pEther->ether_shost));
  pEther->ether_type  = htons(ETHERTYPE_BRN); 
  p->set_ether_header(pEther);

  return (p);
}

////////////////////////////////////////////////////////////////////////////////

Packet* 
BrnIappEncap::create_handover_routeupdate(
  EtherAddress dst,
  EtherAddress src,
  EtherAddress sta,
  EtherAddress ap_new, 
  EtherAddress ap_old,
  uint8_t      seq_no)
{
  BRN_DEBUG("Create route update from %s to %s (sta %s, new %s, old %s, seq %d)", 
    src.unparse().c_str(), dst.unparse().c_str(), sta.unparse().c_str(), 
    ap_new.unparse().c_str(), ap_old.unparse().c_str(), seq_no);

  // Create an empty iapp packet
  size_t size = sizeof(click_ether) + sizeof(click_brn) + sizeof(click_brn_iapp);
  WritablePacket* p = Packet::make(size);
  click_ether*        pEther = (click_ether*)p->data();
  click_brn*          pBrn   = (click_brn*)(pEther+1);
  click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
  click_brn_iapp_ho*  pHo     = &pIapp->payload.ho;

  // Fill iapp header
  pIapp->type         = CLICK_BRN_IAPP_HRU;
  pIapp->payload_len  = 0;
  pHo->seq_no         = seq_no;
  memcpy(pHo->addr_sta,  sta.data(),    sizeof(pHo->addr_sta));
  memcpy(pHo->addr_mold, ap_old.data(), sizeof(pHo->addr_mold));
  memcpy(pHo->addr_mnew, ap_new.data(), sizeof(pHo->addr_mnew));
  
  // Fill brn header
  pBrn->dst_port      = BRN_PORT_IAPP;
  pBrn->src_port      = BRN_PORT_IAPP;
  pBrn->body_length   = htons(sizeof(click_brn_iapp));
  pBrn->ttl           = BRN_ROUTING_MAX_HOP_COUNT;
  pBrn->tos           = BRN_TOS_BE;
  
  // Fill ether header
  memcpy(pEther->ether_dhost, dst.data(), sizeof(pEther->ether_dhost));
  memcpy(pEther->ether_shost, src.data(), sizeof(pEther->ether_shost));
  pEther->ether_type  = htons(ETHERTYPE_BRN); 
  p->set_ether_header(pEther);

  return (p);
}

////////////////////////////////////////////////////////////////////////////////

Packet* 
BrnIappEncap::create_iapp_hello(
  EtherAddress sta,
  EtherAddress ap_cand, 
  EtherAddress ap_curr,
  bool         to_curr)
{
  EtherAddress& src(to_curr ? ap_cand : ap_curr);
  EtherAddress& dst(to_curr ? ap_curr : ap_cand);

  BRN_DEBUG("Create hello from %s to %s (client %s)", 
    src.unparse().c_str(), dst.unparse().c_str(), sta.unparse().c_str());
    
  // Create an empty iapp packet
  size_t size = sizeof(click_ether) + sizeof(click_brn) + sizeof(click_brn_iapp);
  WritablePacket* p = Packet::make(size);
  click_ether*        pEther = (click_ether*)p->data();
  click_brn*          pBrn   = (click_brn*)(pEther+1);
  click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
  click_brn_iapp_he*  pHe    = &pIapp->payload.he;

  // Fill iapp header
  pIapp->type         = CLICK_BRN_IAPP_HEL;
  pIapp->payload_len  = 0;
  pHe->authoritive    = to_curr ? 0 : 1;
  memcpy(pHe->addr_sta,     sta.data(),     sizeof(pHe->addr_sta));
  memcpy(pHe->addr_ap_curr, ap_curr.data(), sizeof(pHe->addr_ap_curr));
  memcpy(pHe->addr_ap_cand, ap_cand.data(), sizeof(pHe->addr_ap_cand));
  
  // Fill brn header
  pBrn->dst_port      = BRN_PORT_IAPP;
  pBrn->src_port      = BRN_PORT_IAPP;
  pBrn->body_length   = htons(sizeof(click_brn_iapp));
  pBrn->ttl           = BRN_ROUTING_MAX_HOP_COUNT;
  pBrn->tos           = BRN_TOS_BE;
  
  // Fill ether header
  memcpy(pEther->ether_dhost, dst.data(), sizeof(pEther->ether_dhost));
  memcpy(pEther->ether_shost, src.data(), sizeof(pEther->ether_shost));
  pEther->ether_type  = htons(ETHERTYPE_BRN); 
  p->set_ether_header(pEther);

  return (p);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappEncap)

////////////////////////////////////////////////////////////////////////////////
