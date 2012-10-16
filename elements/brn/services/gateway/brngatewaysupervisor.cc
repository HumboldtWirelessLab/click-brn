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

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/etheraddress.hh>

// protocol structures
#include <clicknet/ether.h>

// used BRN elements
#include <elements/brn/routing/linkstat/brn2_brnlinktable.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brngateway.hh"
#include "brngatewaysupervisor.hh"


CLICK_DECLS

BRNGatewaySupervisor::BRNGatewaySupervisor() {}

BRNGatewaySupervisor::~BRNGatewaySupervisor() {}

int
BRNGatewaySupervisor::configure (Vector<String> &conf, ErrorHandler *errh) {
  if (cp_va_kparse(conf, this, errh,
                  "BRNGATEWAY", cpkP+cpkM, cpElement, &_gw,
                  cpEnd) < 0)
    return -1;

  if (_gw->cast("BRNGateway") == 0) {
    return errh->error("No element of type BRNGateway specified.");
  }

  return 0;
}

int
BRNGatewaySupervisor::initialize (ErrorHandler *) {
  return 0;
}


/* handle incoming packets */
void
BRNGatewaySupervisor::push(int port, Packet *p) {
  click_ether* ether = (click_ether*) p->ether_header();

	BRN_CHECK_EXPR_RETURN(ether == NULL,
   ("Ether header not available. Killing packet."), p->kill(); return;);
		
  if (_gw->get_gateway() == NULL) {
    BRN_WARN("Got a packet, but this node is no gateway.");

	  if (port == 0) {
      // flow packet which arrived at non-gateway node
      output(2).push(p);
      return;
		}
		else if (port == 1) {
			// non-flow packet
			
			// try find a gateway yourself
			output(1).push(p);
			return;
		}
  }

  output(0).push(p);
  return;
}

EXPORT_ELEMENT(BRNGatewaySupervisor)
CLICK_ENDDECLS
