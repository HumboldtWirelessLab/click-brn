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

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brn2.h"

#include "gateway.h"
#include "brngateway.hh"
#include "brngatewayencap.hh"

CLICK_DECLS

BRNGatewayEncap::BRNGatewayEncap() {}

BRNGatewayEncap::~BRNGatewayEncap() {}

int
BRNGatewayEncap::configure (Vector<String> &conf, ErrorHandler *errh) {
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
BRNGatewayEncap::initialize (ErrorHandler *) {
  return 0;
}


/* handle incoming packets */
void
BRNGatewayEncap::push(int port, Packet *p) {
  UNREFERENCED_PARAMETER(port);
    
  // keep the ether header on the stack as we might kill the packet with p->push()
  const click_ether * etherp = p->ether_header();
  BRN_CHECK_EXPR_RETURN(etherp == NULL,
              ("Ether header not available. Killing packet."), p->kill(); return;);
  click_ether ether = *etherp; 

	

  uint32_t length = p->length();

	// generate a BRN GATEWAY packet as feedback
  // make place for additional headers
  if (WritablePacket *q = p->push(sizeof(click_ether) +
                                  sizeof(click_brn) +
                                  sizeof(brn_gateway))) {
	  // set ether header
   	click_ether *ether_new = (click_ether *) q->data();
   	ether_new->ether_type = htons(ETHERTYPE_BRN);

   	// copy ethernet addess
   memcpy(ether_new->ether_shost, ether.ether_dhost, 6);
   memcpy(ether_new->ether_dhost, ether.ether_shost, 6);
   //	memcpy(ether_new->ether_dhost, &ether, sizeof(ether.ether_dhost) +
    //                                      sizeof(ether.ether_shost));

		// set annotation
    q->set_ether_header(ether_new);

    // set brn header
    click_brn* brn = (click_brn*) (q->data() + sizeof(click_ether));

    brn->src_port = BRN_PORT_GATEWAY;
    brn->dst_port = BRN_PORT_GATEWAY;
    brn->tos = BRN_TOS_BE;
    brn->ttl = 0xff;
    brn->body_length = htons(length + sizeof(brn_gateway));


	  // set brn gateway header
	  brn_gateway* brn_gw = (brn_gateway*) (q->data() + sizeof(click_ether) + sizeof(click_brn));
	
	  // set failed to sent this packet to internet
	  //brn_gw->failed = 0;
	
	  // update metric of this gateway
	  const BRNGatewayEntry *gwe = _gw->get_gateway();
	  if (gwe == NULL) {
      BRN_WARN("This gateway is no gateway anymore.");
		  brn_gw->metric = 0;
    }
	  else {
		  brn_gw->metric = gwe->get_metric();
      BRN_INFO("Sending feedback with metric %u for gw %s", brn_gw->metric, EtherAddress(ether_new->ether_shost).unparse().c_str());
    }

    output(0).push(q);
    return;
  }
  else {
    p->kill();
    return;
  }
}

EXPORT_ELEMENT(BRNGatewayEncap)
CLICK_ENDDECLS
