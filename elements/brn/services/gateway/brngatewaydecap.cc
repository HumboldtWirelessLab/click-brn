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
#include "gateway.h"

// used BRN elements
#include <elements/brn/routing/linkstat/brn2_brnlinktable.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brn2.h"

#include "brngateway.hh"
#include "brngatewaydecap.hh"

CLICK_DECLS

BRNGatewayDecap::BRNGatewayDecap()
 : _gw(NULL)
{
  BRNElement::init();
}

BRNGatewayDecap::~BRNGatewayDecap() {}

int
BRNGatewayDecap::configure (Vector<String> &conf, ErrorHandler *errh) {
  if (cp_va_kparse(conf, this, errh,
                  "BRNGATEWAY", cpkP+cpkM, cpElement, &_gw,
                  "DEBUG", cpkP, cpInteger, &_debug,
                  cpEnd) < 0)
    return -1;

  if (_gw->cast("BRNGateway") == 0) {
    return errh->error("No element of type BRNGateway specified.");
  }

  return 0;
}

int
BRNGatewayDecap::initialize (ErrorHandler *) {
  return 0;
}


/* handle incoming packets
 * 
 * assumes BRN packets
 * 
 * */
void
BRNGatewayDecap::push(int port, Packet *p) {
  UNREFERENCED_PARAMETER(port);

	// get BRN header
	const click_brn *brn = reinterpret_cast<const click_brn *>( p->data());
        
	BRN_CHECK_EXPR_RETURN(brn->dst_port != BRN_PORT_GATEWAY || brn->src_port != BRN_PORT_GATEWAY,
						  ("Got packet BRN packet not for GATEWAY. Killing it. Error in configuration."),
						  p->kill(); return;);

  BRN_INFO("Got feedback packet");      
	
	assert(sizeof(click_brn) + ntohs(brn->body_length) == p->length());
	
	const brn_gateway *brn_gw = reinterpret_cast<const brn_gateway *>( (p->data() + sizeof(click_brn)));
  
  if (p->length() == sizeof(click_brn) + sizeof(brn_gateway)) {
    // just a fake packet to test for the route
    BRN_WARN("Got a test packet from %s", EtherAddress(p->ether_header()->ether_shost).unparse().c_str());
    
    p->kill();
    return;
  }
  
  
	// ether header of packet sent to gw
	const click_ether *ether = reinterpret_cast<const click_ether *>( (p->data() + sizeof(click_brn) + sizeof(brn_gateway)));
	
	EtherAddress chosen_gw = EtherAddress(ether->ether_shost);
	
	uint8_t metric = brn_gw->metric;
	
	if (metric == 0) {
		BRN_INFO("Choosed gw %s failed", chosen_gw.unparse().c_str());      
	
		// remove chosen gateway
		_gw->remove_gateway(chosen_gw);
	}
  else {
		BRNGatewayEntry *gwe;
		
		// extract metric from packet and update in memory
		if ((gwe = _gw->get_gateways()->findp(chosen_gw)) != NULL) {
			BRN_INFO("Stored metric %u for gw %s", metric, chosen_gw.unparse().c_str());
      gwe->set_metric(metric);
		}
	}
		
	// strip header
	p->pull(sizeof(click_brn) + sizeof(brn_gateway));
	
  // and set ether annotation
  p->set_ether_header(ether);
  
	// ... and output
	output(0).push(p);
}

void
BRNGatewayDecap::add_handlers()
{
  BRNElement::add_handlers();
}

EXPORT_ELEMENT(BRNGatewayDecap)
CLICK_ENDDECLS
