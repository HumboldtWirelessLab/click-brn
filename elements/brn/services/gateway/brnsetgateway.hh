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

#ifndef BRNSETGATEWAY_HH
#define BRNSETGATEWAY_HH

#include <click/bighashmap.hh>
#include "elements/brn/routing/standard/routemaintenance/routemaintenance.hh"

#include "brngateway.hh"
CLICK_DECLS

/*
 * =c
 * BRNSetGateway([GW etheraddress])
 * =d
 * 
 * This element marks the gateway for a packet to be sent to.
 *
 */

class BRNSetGateway : public Element {
public:

    BRNSetGateway();
    ~BRNSetGateway();

    const char *class_name() const { return "BRNSetGateway"; }

    const char *port_count() const {
        return "1/2";
    }

    const char *processing() const {
        return PUSH;
    }

    int initialize(ErrorHandler *);
    int configure(Vector<String> &conf, ErrorHandler *errh);

    void push(int, Packet *);

    int _debug;
private:
    void handle_feedback(Packet *p);
    void set_gateway(Packet *p);

    const EtherAddress choose_gateway();
    void set_gateway_on_packet(Packet *, const EtherAddress*);

    BRNGateway *_gw; // the gateway element, which stores infos about known hosts
    RoutingMaintenance *_routing_maintenance; // link table to determine metric to available gateways
};

CLICK_ENDDECLS
#endif
