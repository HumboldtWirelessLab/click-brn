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
#include "vlandecap.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include "elements/brn/brn.h"

CLICK_DECLS

VLANDecap::VLANDecap()
{
}

VLANDecap::~VLANDecap()
{
}

int
VLANDecap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}


Packet *
VLANDecap::simple_action(Packet *p)
{
  click_ether_vlan *vlanh = (click_ether_vlan *) p->data();
  
  if (vlanh == NULL) {
  	//click_chatter("%{element}: ERROR No ether header found. Killing packet. ", this);
  	p->kill();
  	return NULL;
  }
  
  if (ntohs(vlanh->ether_vlan_proto) == ETHERTYPE_BRN) {
  	//click_chatter("%{element}: ERROR in configuration got BRN packet. Fix me.", this);
  	return p;
  }
  
  if (ntohs(vlanh->ether_vlan_proto) != ETHERTYPE_8021Q) {
  	//click_chatter("%{element}: WARN No vlan header found. Just return the packet. Check this policy.", this);
  	return p;
  }

  if (WritablePacket *q = p->uniqueify()) {
  	
  	click_ether * etherh = (click_ether *) (q->data() + (sizeof(click_ether_vlan) - sizeof(click_ether)));
  	
  	// move ether addresses
  	memmove(etherh->ether_dhost, etherh->ether_dhost - (sizeof(click_ether_vlan) - sizeof(click_ether)), sizeof(vlanh->ether_shost) + sizeof(vlanh->ether_dhost));
    
    q->set_ether_header(etherh);
  	
  	q->pull(sizeof(click_ether_vlan) - sizeof(click_ether));
  	
  	return q;
  	
  } else {
  	//click_chatter("%{element}: ERROR Failed uniqueify", this);
    return NULL;
  }
}

EXPORT_ELEMENT(VLANDecap)
CLICK_ENDDECLS
