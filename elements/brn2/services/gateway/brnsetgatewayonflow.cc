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
 * brnsetgatewayonflow.{cc,hh} -- element chooses a gateway for packet
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>

// protocol structures
#include <clicknet/ether.h>
#include <clicknet/ip.h>

// elements
#include <elements/analysis/aggregateipflows.hh>
#include <elements/ethernet/arptable.hh>

// used BRN elements
#include <elements/brn2/routing/linkstat/brn2_brnlinktable.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brn2.h"

#include "brnsetgatewayonflow.hh"
#include "brngateway.hh"
#include "brnpacketbuffer.hh"



CLICK_DECLS


BRNSetGatewayOnFlow::BRNSetGatewayOnFlow()
{
  BRNElement::init();
}

BRNSetGatewayOnFlow::~BRNSetGatewayOnFlow() {}

int
BRNSetGatewayOnFlow::configure (Vector<String> &conf, ErrorHandler *errh) {
  if (cp_va_kparse(conf, this, errh,
                  "BRNGATEWAY", cpkP+cpkM, cpElement,/* "BRNGateway",*/ &_gw,
                  "AGGIPFLOWS", cpkP+cpkM, cpElement,/* "AggregateIPFlows",*/ &_aggflows,
                  "ARPTABLE", cpkP+cpkM, cpElement,/* "ARPTable",*/ &_arp,
                  "PACKETBUFFER", cpkP+cpkM, cpElement,/* "BRNPacketBuffer",*/ &_buffer,
                  "IP", cpkP+cpkM, cpIPPrefix,/* "src IP address with prefix",*/ &_src_ip, &_src_ip_mask,
                  "ROUTINGMAINTENANCE", cpkP+cpkM, cpElement, &_routing_maintenance,
                  cpEnd) < 0)
      return -1;


  if (_gw->cast("BRNGateway") == 0) {
      return errh->error("No element of type BRNGateway specified.");
  }

  if (_aggflows->cast("AggregateIPFlows") == 0) {
      return errh->error("No element of type AggregateIPFlows specified.");
  }

  if (_arp->cast("ARPTable") == 0) {
      return errh->error("No element of type ARPTable specified.");
  }

  if (_buffer->cast("BRNPacketBuffer") == 0) {
      return errh->error("No element of type BRNPacketBuffer specified.");
  }

  return 0;
}

int
BRNSetGatewayOnFlow::initialize (ErrorHandler *) {
	
	_aggflows->add_listener(this);
	
  return 0;
}


const EtherAddress
BRNSetGatewayOnFlow::choose_gateway() {
  // the gateway's metric to reach it and the gateway metric itself
  EtherAddress best_gw = EtherAddress();
  uint8_t best_gw_metric = 0xFF;
  uint32_t best_metric_to_reach_gw = 0xFFFFFFFF;
  uint32_t metric;

  const BRNGatewayList *list_of_gws = _gw->get_gateways();

  if (list_of_gws->empty()) {
      BRN_INFO("No Gateway available.");
      return best_gw;
  }

	// TODO
	// this is expensive for every packet
	// I should remember a list of best gateways

  // iterate over known gateways
  for (BRNGatewayList::const_iterator i = list_of_gws->begin(); i.live(); i++) {
    // is connection to this gateway better or equal than a already found one
    // equal is added to ensure that the values are copied even, if the gateway is reachable with 0xFFFFFFFF
    // (which may mean, that we just don't know this node yet; but since it reaches the dht it must be reachable at all)

    const EtherAddress gw = i.key();
    const BRNGatewayEntry gwe = i.value();
    unsigned new_metric;

    BRN_DEBUG("Running over gateway %s", gw.unparse().c_str());

    // find a route
    Vector<EtherAddress> route;
    _routing_maintenance->best_route_from_me(gw, route, &metric);

    if (!_routing_maintenance->valid_route(route) && !(_gw->_my_eth_addr == gw)) {
		  BRN_DEBUG("Don't know metric to gateway %s.", gw.unparse().c_str());

      if (_routing_maintenance->valid_route(route)) {
        // have a route now
        break; 
      }

      // TODO
		  // store a list of lastly used gateways
		  // those are the best gateway for this node
		  // improve lookup
			// but update round robin from time to time
		
			// fire a packet for testing to gateways we don't have a route to
		
	    // packet for checking the route
  	  if (WritablePacket* p = Packet::make(sizeof(click_ether) + sizeof(click_brn) + sizeof(brn_gateway))) {
    		// set ether header
 				click_ether *ether = (click_ether *) p->data();
 				ether->ether_type = htons(ETHERTYPE_BRN);

 				memcpy(ether->ether_shost, _gw->_my_eth_addr.data(), 6);
  			memcpy(ether->ether_dhost, gw.data(), 6);
	
				// set annotation
  			p->set_ether_header(ether);

  			// set brn header
  			click_brn* brn = (click_brn*) (p->data() + sizeof(click_ether));

  			brn->src_port = BRN_PORT_GATEWAY;
  			brn->dst_port = BRN_PORT_GATEWAY;
  			brn->tos = BRN_TOS_BE;
  			brn->ttl = 0xff;
  			brn->body_length = htons(sizeof(brn_gateway));

				// set brn gateway header
				//brn_gateway* brn_gw = (brn_gateway*) (p->data() + sizeof(click_ether) + sizeof(click_brn));

				// set failed to sent this packet to internet
				//brn_gw->failed = 0;
      
      	BRN_DEBUG("Sending packet to %s to test metric.", gw.unparse().c_str());
				output(0).push(p);
    	}
    	continue;
  	}
      
    // is metric to found gateway better than to old gateway
    new_metric = _routing_maintenance->get_route_metric(route);
      
    if ((_gw->_my_eth_addr == gw) || ((new_metric
              < best_metric_to_reach_gw) && (gwe.get_metric() != 0))) { // TODO better combination of metric to gateway and gateway metric may be nedded
      BRN_DEBUG("Gateway %s (%u) is better than %s (%u)", gw.unparse().c_str(), new_metric, best_gw.unparse().c_str(), best_metric_to_reach_gw);
      // save new best gateway
      best_metric_to_reach_gw = new_metric;
      best_gw_metric = gwe.get_metric();
      best_gw = gw;
    }
      
    BRN_DEBUG("Current best gateway is %s with metric %u", best_gw.unparse().c_str(), best_metric_to_reach_gw);
  }

  BRN_INFO("Choose gateway %s which is reachable with a metric of %u.",
  					best_gw.unparse().c_str(), best_metric_to_reach_gw);
  return best_gw;
}


/*
 *
 */
void
BRNSetGatewayOnFlow::push(int port, Packet *p) {
  
  if (port == 0) {
	  // checking headers
	  click_ether* ether = (click_ether*) p->ether_header();
	
	  BRN_CHECK_EXPR_RETURN(ether == NULL,
	              ("Ether header not available. Killing packet."), p->kill(); return;);
	    
	  BRN_CHECK_EXPR_RETURN(htons(ether->ether_type) != ETHERTYPE_IP,
	              ("No IP packet. Killing it. I shouldn't get non-IP packets. Error in click configuration. But got 0x%x", htons(ether->ether_type)), p->kill(); return;);
	    
	  click_ip *ip = (click_ip *) p->ip_header();
	
	  BRN_CHECK_EXPR_RETURN(ip == NULL,
	              ("No IP header found. Killing it."), p->kill(); return;);
	
	  //	
	  // packet to set a gateway for
	  //
	  
	  uint32_t agg = get_aggregate(p);
	  
	  EtherAddress gw;
	  IPFlowID flow = IPFlowID(p);
	    
	  // get gateway for flow
	  if (!(gw = _flow2gw.find(agg, EtherAddress()))) {
		  // no gateway found for flow => choose a gateway
	  
	    // check, if flow is known from handover
	    if (_flows_handover.findp(flow) != NULL) {
	      // found flow in hand over
	      gw = _flows_handover.find(flow);
	      _flows_handover.remove(flow);
	      
	      BRN_INFO("Flow %s and its gateway %s known from handover. Using gw %s.", flow.unparse().c_str(), gw.unparse().c_str(), gw.unparse().c_str());
	    }
	    else {
	      BRN_INFO("Flow %s unknown => choosing a gateway", flow.unparse().c_str());
	      gw = choose_gateway();
	    }
	    
	    if (!gw) {
	      // no gateway to choose
	      BRN_INFO("No Gateway found. Sending packet to port 1");
	      output(1).push(p);
	      return;
	    }    
	    	
	    // insert choosen gateway for flow 
	    if (!(_flow2gw.insert(agg, gw))) {
	      BRN_ERROR("Could not insert new gateway for flow");
	    }
	       	
	    BRN_INFO("Inserting gw %s for flow %s (#flows = %u)", gw.unparse().c_str(), _flows.find(agg).unparse().c_str(), _flow2gw.size());
	  }
	  else
	  {
	    if (_flows_handover.findp(flow) != NULL) {
	      _flows_handover.remove(flow);
	      BRN_INFO("Got a packet for handed over flow %s. Removing this handed over flow.", flow.unparse().c_str());
	    }
	    
	    BRN_DEBUG("Using existing gw %s for flow %s (#flows = %u)", gw.unparse().c_str(), _flows.find(agg).unparse().c_str(), _flow2gw.size());
	  }
	  
	  // test whether the chosen gateway is really a choosable gateway
	  if (_gw->get_gateways()->findp(gw) == NULL) {
	    BRN_INFO("Gateway %s not known on this node. Must be known from handover and gateways from dht not refreshed yet.",
	              gw.unparse().c_str());
	  }
	  
	  // now a gateway is set
	  if (WritablePacket* q = p->uniqueify()) {
	    // rewrite packet with new gateway
	    click_ether *ether = q->ether_header();
	    memcpy(ether->ether_dhost, gw.data(), 6);
	    BRN_DEBUG("Rewriting packet for gateway %s and sending it to port 0", gw.unparse().c_str());
	    output(0).push(q);
	    return;
	  }
  }
  else if (port == 1) {  
    BRN_INFO("Getting handover packets");
    
    /*click_brn_iapp*     pIapp  = (click_brn_iapp*)p->data();
    
    BRN_CHECK_EXPR_RETURN(CLICK_BRN_IAPP_PAYLOAD_GATEWAY != pIapp->payload_type,
          ("got invalid iapp payload type %d", pIapp->payload_type), if (p) p->kill(); return;);
    click_brn_iapp_ho* pHo = &pIapp->payload.ho;
    
    switch (pIapp->type) {
      case CLICK_BRN_IAPP_HON: {
        BRN_INFO("Receiving handover notify from port 1");
        EtherAddress client(pHo->addr_sta);
  
        // remove flows that where passed during a hand over
        // because client moved away
        remove_handover_flows_from_client(client);
    
        p->kill();
        return;
      }
      case CLICK_BRN_IAPP_HOR: {
        BRN_INFO("Receiving handover reply from port 1");

        // extract gateway information
        
        BRN_DEBUG("Extracting flow information");
          
        brn_gateway_handover* gw_handover = (brn_gateway_handover*) (pIapp + 1);
        
        uint16_t length = ntohs(gw_handover->length);
        assert(length % 18 == 0);
        uint16_t size = length / 18;
        flow_gw flow;
        
        for (int i = 0; i < size; ++i) {
          flow = gw_handover->flows[i];
          
          IPFlowID f = IPFlowID(ntohl(flow.src),
                                ntohs(flow.sport),
                                ntohl(flow.dst),
                                ntohs(flow.dport));
          EtherAddress gw = EtherAddress(flow.gw);
          
          add_handover_flow_from_client(f, gw);
          
          BRN_INFO("Handed over flow %s with gateway %s.", f.unparse().c_str(), gw.unparse().c_str());
        
          uint32_t agg = 0;
          agg = _flow2agg.find(f, 0);
          if (agg != 0) {
            // client has sent already packets for this flow, which are buffered
            // => flush packets from buffer          
            _buffer->flush_bucket(agg);
            BRN_INFO("Notified packet buffer to flush packets with flow %s (bucket %u)", f.unparse().c_str(), agg);
          }
          else {
            BRN_INFO("Flow %s not yet known. Don't need to flush packet buffer.", f.unparse().c_str());
          }
        }
        p->kill();
        return;
      }
      default: {
        p->kill();
        return;
      }
    }
  */}
  else if (port == 2) {
    // add gateway information 
    BRN_DEBUG("Receiving handover reply from port 2");
  
    /*click_ether*        pEther = (click_ether*)p->data();
    click_brn*          pBrn   = (click_brn*)(pEther+1);
    click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
    click_brn_iapp_ho*  pHo    = &pIapp->payload.ho;

    EtherAddress client(pHo->addr_sta);
  
    if (pIapp->type == CLICK_BRN_IAPP_HOR) {
      BRN_INFO("Adding flow information");
      
      // get connections of client
      FlowsHandover flows = get_flows_from_client(client);
  
      // length of payload
      uint16_t length = flows.size() * sizeof(flow_gw);
  
      // put flows into packet
      if (WritablePacket* q = p->put(length + sizeof(length))) {
    
        IPFlowID flow;
        click_ether*        pEther = (click_ether*)q->data();
        click_brn*          pBrn   = (click_brn*)(pEther+1);
        click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
        pIapp->payload_type = CLICK_BRN_IAPP_PAYLOAD_GATEWAY;
    
        brn_gateway_handover* gw_handover = (brn_gateway_handover*) (pIapp + 1);
        gw_handover->length = htons(length);
    
        flow_gw* flowgw;
  
        int num_flow = 0;
    
        for (FlowsHandover::const_iterator i = flows.begin(); i.live(); i++) {
          flow = i.key();
      
          flowgw = &gw_handover->flows[num_flow];
          flowgw->src = htonl(flow.saddr().addr());
          flowgw->sport = htons(flow.sport());
          flowgw->dst = htonl(flow.daddr().addr());
          flowgw->dport = htons(flow.dport());
      
          memcpy(flowgw->gw, i.value().data(), 6);
      
          ++num_flow;
        }
        BRN_DEBUG("Sending handover reply to port 2");
        output(2).push(q);
      }
      else {
        BRN_WARN("Extending packet failed");
        p->kill();
        return;
      }
    }
    else {
      BRN_DEBUG("Wasn't a handover reply. Passing unmodified.");
      output(2).push(p);
    }*/
  } 
}

uint32_t
BRNSetGatewayOnFlow::get_aggregate(const Packet *p) {
  // extract flow from packet
  uint32_t agg = AGGREGATE_ANNO(p);
  BRN_CHECK_EXPR(agg == 0,
      ("No aggregate annotation found. Error in Click configuration. Serious problem. Fix it."));
  return agg;
}

/**
 * 
 * Remove connections flowing through the given EthernetAddress
 * 
 * 
 */
void
BRNSetGatewayOnFlow::remove_flows_with_gw(EtherAddress eth) {
  for (FlowGateways::const_iterator i = _flow2gw.begin(); i.live(); i++) {
    if (i.value() == eth) {
      _flow2gw.remove(i.key());
      _flow2agg.remove(_flows.find(i.key()));
      _flows.remove(i.key());
    }
  }
}

/**
 * 
 * Get connections for a given client
 * 
 * 
 */
FlowsHandover
BRNSetGatewayOnFlow::get_flows_from_client(EtherAddress eth) {
  IPAddress client = _arp->reverse_lookup(eth);    // get client's IP address

  FlowsHandover client_flows;
  IPFlowID flow;

  for (Flows::const_iterator i = _flows.begin(); i.live(); i++) {
    flow = i.value();
    if (flow.saddr() == client) {
      client_flows.insert(flow, _flow2gw.find(i.key()));
    }
  }

  return client_flows;
}

/**
 * 
 * Add given flow with gw from handover
 * 
 * 
 */
bool
BRNSetGatewayOnFlow::add_handover_flow_from_client(IPFlowID flow, EtherAddress gw) {
  return _flows_handover.insert(flow, gw);
}

/**
 * 
 * Remove flows from handover from given client
 * 
 * 
 */
void
BRNSetGatewayOnFlow::remove_handover_flows_from_client(EtherAddress eth) {
  IPAddress client = _arp->reverse_lookup(eth);  // get client's IP address

  for (FlowsHandover::const_iterator i = _flows_handover.begin(); i.live(); i++) {
    if (i.key().saddr() == client) {
      _flows_handover.remove(i.key());
    }
  }
}

/**
 * 
 * 
 * 
 */
uint32_t
BRNSetGatewayOnFlow::get_bucket(const Packet *p) {
  // for each flow on bucket
  return get_aggregate(p);
}

/**
 * 
 * 
 * 
 */
bool
BRNSetGatewayOnFlow::buffer_packet(const Packet *p) {
  IPFlowID flow = IPFlowID(p);
  bool buffer = true;

	// TODO
	// need to check, if station is associated longer than <max time
	// to handoff information about flows>
	// then we don't need to buffer

	// don't buffer, if flow is already known from handover
	if (_flows_handover.findp(flow) != NULL)
		buffer = false;
	// don't buffer, if gateway is already known
	if (_flow2gw.findp(get_aggregate(p)) != NULL)
		buffer = false;

  
  BRN_DEBUG("buffer_packet called. Buffer packet? %u", buffer);
  return buffer;
}

/**
 * 
 * 
 * 
 */
void
BRNSetGatewayOnFlow::aggregate_notify(uint32_t agg, AggregateEvent event, const Packet *p)
{
  switch (event) {
    case NEW_AGG: {
      IPFlowID flow = IPFlowID(p);

      // quick fix
      // I may get the the reverse flow packet first
      if (!flow.saddr().matches_prefix(_src_ip, _src_ip_mask)) {
        BRN_INFO("Rewriting flow %s.", flow.unparse().c_str());
        flow = IPFlowID(flow.daddr(), flow.dport(), flow.saddr(), flow.sport());
      }

      _flows.insert(agg, flow);
      _flow2agg.insert(flow, agg);

      BRN_INFO("Adding flow: %s with agg = %u", flow.unparse().c_str(), agg);
      break;
    }
    case DELETE_AGG: {
      IPFlowID flow = _flows.find(agg);
			BRN_INFO("Removing flow: %s with agg = %u and its gw %s", flow.unparse().c_str(), agg, _flow2gw.find(agg).unparse().c_str());
			_flows.remove(agg);
      _flow2agg.remove(flow);
			_flow2gw.remove(agg);
			break;
		}
	}
}


/*
 *
 * Handlers
 *
 */

enum { HANDLER_FLOWS, HANDLER_FLOWS_HAND_OVER, HANDLER_REMOVE_FLOWS };


/*
 *
 * return the known gateway connection
 *
 */
String
BRNSetGatewayOnFlow::read_handler(Element *e, void *thunk) {
  BRNSetGatewayOnFlow *gws = static_cast<BRNSetGatewayOnFlow *> (e);
  switch ((uintptr_t) thunk) {
  case HANDLER_FLOWS: {
	  StringAccum sa;
	  sa << "Flow\t\t\t\tGateway\n";
	  // iterate over all known gateways
	  for (FlowGateways::const_iterator i = gws->_flow2gw.begin(); i.live(); i++) {
	      uint32_t agg = i.key();
	      IPFlowID flow = gws->_flows.find(agg);
	      //assert(flow);
	      sa << flow.unparse() << "\t" << i.value().unparse() << "\n";
	  }
	  sa << "Tracking " << (uint32_t) gws->_flow2gw.size() << " connections.\n";
	  return sa.take_string();
  }
  case HANDLER_FLOWS_HAND_OVER: {
	  StringAccum sa;
	  sa << "Flow\t\t\t\tGateway\n";
	  // iterate over all known gateways
	  for (FlowsHandover::const_iterator i = gws->_flows_handover.begin(); i.live(); i++) {
	      sa << i.key().unparse() << "\t" << i.value().unparse() << "\n";
	  }
	  sa << (uint32_t) gws->_flows_handover.size() << " connections are handed over.\n";
	  return sa.take_string();
  }
  default:
    return "<error>\n";
  }
}

int
BRNSetGatewayOnFlow::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh) {
  BRNSetGatewayOnFlow *gsw = static_cast<BRNSetGatewayOnFlow *> (e);
  String s = cp_uncomment(data);
  switch ((intptr_t)thunk) {
  case HANDLER_REMOVE_FLOWS: {

    EtherAddress gw;

    if (cp_va_kparse(data, gsw, errh,
                          "GATEWAYSMAC", cpkP, cpEtherAddress, &gw,
                          cpEnd) < 0)
        return errh->error("wrong arguments to handler 'remove_flows'");

    gsw->remove_flows_with_gw(gw);
    return 0;
  }
  default:
    return errh->error("internal error");
  }
}

void
BRNSetGatewayOnFlow::add_handlers() {
  BRNElement::add_handlers();

  add_read_handler("flows", read_handler, (void *) HANDLER_FLOWS);
  add_read_handler("flows_handed_over", read_handler, (void *) HANDLER_FLOWS_HAND_OVER);
  add_write_handler("remove_flows", write_handler, (void *) HANDLER_REMOVE_FLOWS);
}

ELEMENT_REQUIRES(AggregateNotifier)
EXPORT_ELEMENT(BRNSetGatewayOnFlow)
CLICK_ENDDECLS
