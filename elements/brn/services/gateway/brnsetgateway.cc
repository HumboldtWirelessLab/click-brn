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
 * brnsetgateway.{cc,hh} -- element chooses a gateway for packet
 *
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/straccum.hh>

// protocol structures
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include "gateway.h"

// used BRN elements
#include <elements/brn/routing/linkstat/brn2_brnlinktable.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brn2.h"


#include "brnsetgateway.hh"
#include "brngateway.hh"


CLICK_DECLS


BRNSetGateway::BRNSetGateway() :
 _debug(0),_gw(NULL),_routing_maintenance(NULL)
{}

BRNSetGateway::~BRNSetGateway() {}

int
BRNSetGateway::configure (Vector<String> &conf, ErrorHandler *errh) {
  if (cp_va_kparse(conf, this, errh,
      "BRNGATEWAY", cpkP+cpkM, cpElement, &_gw,
      "ROUTINGMAINTENANCE", cpkP+cpkM, cpElement, &_routing_maintenance,
      cpEnd) < 0)
    return -1;


  if (_gw->cast("BRNGateway") == 0) {
    return errh->error("No element of type BRNGateway specified.");
  }

  return 0;
}

int
BRNSetGateway::initialize (ErrorHandler *) {
  return 0;
}


const EtherAddress
BRNSetGateway::choose_gateway() {
  // the gateway's metric to reach it and the gateway metric itself
  EtherAddress best_gw = EtherAddress();
  //uint8_t best_gw_metric = 0xFF;
  uint32_t best_metric_to_reach_gw = 0xFFFFFFFF;

  const BRNGatewayList *list_of_gws = _gw->get_gateways();

  if (list_of_gws->empty()) {
    BRN_WARN("No Gateway available.");
    return best_gw;
  }

	// TODO
	// this is expensive for every packet
	// Should remember a list of best gateways?

  // iterate over known gateways
  for (BRNGatewayList::const_iterator i = list_of_gws->begin(); i.live();++i) {
   // is connection to this gateway better or equal than a already found one
   // equal is added to ensure that the values are copied even, if the gateway is reachable with 0xFFFFFFFF
   // (which may mean, that we just don't know this node yet; but since it reaches the dht it must be rachable at all)

   const EtherAddress gw = i.key();
   const BRNGatewayEntry gwe = i.value();
   unsigned new_metric;
   uint32_t metric;

   // find a route
   Vector<EtherAddress> route;
   _routing_maintenance->best_route_from_me(gw, route, &metric);

   if (!_routing_maintenance->valid_route(route) && !(_gw->_my_eth_addr == gw)) {

     BRN_WARN(" Routes are %s", _routing_maintenance->print_routes(true).c_str());

     if (_routing_maintenance->valid_route(route)) {
       // have a route now
       break; 
     }

     BRN_WARN(" Routes are\n%s", _routing_maintenance->print_routes(true).c_str());

     // TODO
     // store a list of lastly used gateways
     // those are the best gateway for this node
     // improve lookup
     // but update round robin from time to time

     BRN_WARN("Sending packet to %s to test metric.", gw.unparse().c_str());

     // packet for checking the route
     if (WritablePacket* p = Packet::make(sizeof(click_ether) + sizeof(click_brn) + sizeof(brn_gateway))) {
       // set ether header
       click_ether *ether = reinterpret_cast<click_ether *>( p->data());
       ether->ether_type = htons(ETHERTYPE_BRN);

       memcpy(ether->ether_shost, _gw->_my_eth_addr.data(), 6);
       memcpy(ether->ether_dhost, gw.data(), 6);

       // set annotation
       p->set_ether_header(ether);


       // set brn header
       click_brn* brn = reinterpret_cast<click_brn*>( (p->data() + sizeof(click_ether)));

       brn->src_port = BRN_PORT_GATEWAY;
       brn->dst_port = BRN_PORT_GATEWAY;
       brn->tos = BRN_TOS_BE;
       brn->ttl = 0xff;
       brn->body_length = htons(sizeof(brn_gateway));

       // set brn gateway header
       //brn_gateway* brn_gw = reinterpret_cast<brn_gateway*>( (p->data() + sizeof(click_ether) + sizeof(click_brn)));

       // set failed to sent this packet to internet
       //brn_gw->failed = 0;

       output(0).push(p);
     }

     continue;
   }

   // is metric to found gateway better than to old gateway
   new_metric = _routing_maintenance->get_route_metric(route);

   if ((_gw->_my_eth_addr == gw) || ((new_metric < best_metric_to_reach_gw) && (gwe.get_metric() != 0))) {
     // TODO better combination of metric to gateway and gateway metric may be nedded
     // save new best gateway
     best_metric_to_reach_gw = new_metric;
     //best_gw_metric = gwe.get_metric();
     best_gw = gw;
   }
  }

  BRN_INFO("Choose gateway %s which is reachable with a metric of %u.", best_gw.unparse().c_str(),
                                                                        best_metric_to_reach_gw);
  return best_gw;
}

/**
 *
 * Chooses a gateway and rewrites the packet
 *
 */

void
BRNSetGateway::set_gateway_on_packet(Packet *p_in, const EtherAddress *gw) {

  WritablePacket* p = p_in->uniqueify();

  if (!*gw) {
      // no gateway
      BRN_INFO("No Gateway found. Sending packet to port 1");
      output(1).push(p);
      return;
  }

  // rewrite packet with new gateway
  click_ether *ether = p->ether_header();
  memcpy(ether->ether_dhost, gw->data(), 6);
  BRN_INFO("Rewriting packet for gateway %s and sending it to port 0", gw->unparse().c_str());
  output(0).push(p);
}

void
BRNSetGateway::set_gateway(Packet *p)
{
    EtherAddress gw = choose_gateway();
    set_gateway_on_packet(p, &gw);
  return;
}


/* handle incoming packets */
void
BRNSetGateway::push(int port, Packet *p) {
  UNREFERENCED_PARAMETER(port);

  // checking headers
  const click_ether* ether = reinterpret_cast<const click_ether*>( p->ether_header());

  BRN_CHECK_EXPR_RETURN(ether == NULL,
            ("Ether header not available. Killing packet."), p->kill(); return;);

  BRN_CHECK_EXPR_RETURN(htons(ether->ether_type) != ETHERTYPE_IP,
            ("No IP packet. Killing it. I should get non-IP packets. "
             "Error in click configuration."), p->kill(); return;);

  const click_ip *ip = reinterpret_cast<const click_ip *>( p->ip_header());

  BRN_CHECK_EXPR_RETURN(ip == NULL, ("No IP header found. Killing it."), p->kill(); return;);

  set_gateway(p);
}

EXPORT_ELEMENT(BRNSetGateway)
CLICK_ENDDECLS
