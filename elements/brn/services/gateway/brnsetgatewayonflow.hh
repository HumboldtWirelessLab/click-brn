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

#ifndef BRNSETGATEWAYONFLOW_HH
#define BRNSETGATEWAYONFLOW_HH

#include <click/bighashmap.hh>
#include <click/ipflowid.hh>
#include <click/etheraddress.hh>

#include <elements/analysis/aggregatenotifier.hh>
#include "elements/brn/routing/standard/routemaintenance/routemaintenance.hh"
#include <elements/brn/brnelement.hh>

#include "brnpacketbuffertrigger.hh"

// TODO
// move gateway.h> out of here and to c file; but used by BrnIaapNotifyHandler
// defines the protocol structures
#include "gateway.h"

CLICK_DECLS

class AggregateIPFlows;
class ARPTable;
class BRNGateway;
class BRNPacketBuffer;


/*
 * =c
 * BRNSetGatewayOnFlow([GW etheraddress])
 * =d
 * 
 * This element sets the gateway for a packet.
 *
 */


typedef HashMap<uint32_t, IPFlowID> Flows;
typedef HashMap<IPFlowID, uint32_t> FlowAggregate;
typedef HashMap<uint32_t, EtherAddress> FlowGateways;
typedef HashMap<IPFlowID, EtherAddress> FlowsHandover;

class BRNSetGatewayOnFlow : public BRNElement, public AggregateListener, public BRNPacketBufferTrigger {
public:

    BRNSetGatewayOnFlow();
    ~BRNSetGatewayOnFlow();

    const char *class_name() const { return "BRNSetGatewayOnFlow"; }

    const char *port_count() const {
        return "3/3";
    }

    const char *processing() const {
        return PUSH;
    }

    int initialize(ErrorHandler *);
    int configure(Vector<String> &conf, ErrorHandler *errh);

    void push(int, Packet *);

    /* handler stuff */
    void add_handlers();

	  // AggregateListener 
	  void aggregate_notify(uint32_t, AggregateEvent, const Packet *);

    // BRNPacketBufferTrigger
    uint32_t get_bucket(const Packet *);
    bool buffer_packet(const Packet *);

    void remove_flows_with_gw(EtherAddress);
    
    FlowsHandover get_flows_from_client(EtherAddress);
    bool add_handover_flow_from_client(IPFlowID, EtherAddress);
    void remove_handover_flows_from_client(EtherAddress);

private:
    const EtherAddress choose_gateway();
    uint32_t get_aggregate(const Packet*);
  
    BRNGateway *_gw; // the gateway element, which stores infos about known hosts
    RoutingMaintenance *_routing_maintenance;

    AggregateIPFlows *_aggflows;
    ARPTable *_arp; 
    
    Flows _flows;
    FlowGateways _flow2gw; // store a gateway per flow
    FlowsHandover _flows_handover; // stores flows that where passed during a hand over
    FlowAggregate _flow2agg; // get agg for given flow

    BRNPacketBuffer *_buffer;
    
    
    IPAddress _src_ip;
    IPAddress _src_ip_mask;

    /*
    	Timer _timer_retry_choose_gateway;
        int _retry_choose_gateway_interval;
        #define DEFAULT_RETRY_CHOOSE_GATEWAY_INTERVAL	20 // default interval in seconds
        static void static_timer_retry_choose_gateway(Timer *, void *);
        */

    static String read_handler(Element*, void*);
    static int write_handler(const String &, Element*, void*, ErrorHandler*);
};


CLICK_ENDDECLS
#endif
