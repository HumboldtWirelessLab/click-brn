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

#ifndef BRNGATEWAYSUPERVISOR_H_
#define BRNGATEWAYSUPERVISOR_H_

//#include <elements/brn/brnelement.hh>
#include <click/bighashmap.hh>
#include <elements/brn/routing/linkstat/brn2_brnlinktable.hh>

CLICK_DECLS

class BRNGateway;
class BrnLinkTable;

/*
 * 
 * controls frames sent to gateway
 * if the gateway has gone down
 * a BRN GATEWAY packet is sent to sender and the ethernet frame is piggy backed
 *
 * Do I have do generate an ICMP error?
 * I think no.
 */

class BRNGatewaySupervisor : public Element {
public:

    BRNGatewaySupervisor();
    ~BRNGatewaySupervisor();

    const char *class_name() const { return "BRNGatewaySupervisor"; }

    const char *port_count() const {
        return "2/3";
    }

    const char *processing() const {
        return PUSH;
    }

    int initialize(ErrorHandler *);
    int configure(Vector<String> &conf, ErrorHandler *errh);

    void push(int, Packet *);

    int _debug;
private:
    BRNGateway *_gw; // the gateway element, which stores infos about known hosts
    Brn2LinkTable *_link_table;
};


CLICK_ENDDECLS
#endif /*BRNGATEWAYSUPERVISOR_H_*/
