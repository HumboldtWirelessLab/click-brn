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

#ifndef BRNICMPPINGSOURCEGATEWAYTESTER_HH
#define BRNICMPPINGSOURCEGATEWAYTESTER_HH

#include <elements/icmp/icmpsendpings.hh>
#include "brngateway.hh"

CLICK_DECLS

/*
 * =c
 * BRNICMPPingSourceGatewayTester()
 * =d
 * 
 * This element extends the ICMPPingSource
 *
 */

class BRNICMPPingSourceGatewayTester : public ICMPPingSource {
public:

    BRNICMPPingSourceGatewayTester();
    ~BRNICMPPingSourceGatewayTester();

    const char *class_name() const { return "BRNICMPPingSourceGatewayTester"; }
    //int configure_phase() const                   { return CONFIGURE_PHASE_FIRST; }
    int configure(Vector<String> &conf, ErrorHandler *errh);
    void push(int, Packet *);

    // see http://pdos.csail.mit.edu/click/doxygen/classElement.html#0c9dc9afd5954fe40f61f31d88320d6b
    void * cast(const char *name);

private:

    BRNGateway *_gw;
    uint8_t _old_metric;
};

CLICK_ENDDECLS
#endif
