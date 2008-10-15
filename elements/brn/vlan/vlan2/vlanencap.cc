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
#include "vlanencap.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include "elements/brnnew/brn.h"

CLICK_DECLS

VLANEncap::VLANEncap()
{
}

VLANEncap::~VLANEncap()
{
}

int
VLANEncap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
VLANEncap::smaction(Packet *p)
{
  click_ether *ether = (click_ether *) p->ether_header();
  
  if (ether == NULL)
  {
  	//click_chatter("ERROR No ether header found. Killing packet. ");
  	p->kill();
  	return NULL;
  }
  
  if (ntohs(ether->ether_type) == ETHERTYPE_8021Q) {
  	//click_chatter("%{element}: WARN Packet has already a VLAN header. Return packet.", this);
  	return p;
  }
  
  
  if (WritablePacket *q = p->push(sizeof(click_ether_vlan) - sizeof(click_ether))) {
  	
  	click_ether_vlan * vlanh = (click_ether_vlan *) q->data();
  	
  	// move ether addresses to beginning of frame
  	memmove(vlanh->ether_dhost, vlanh->ether_dhost + sizeof(click_ether_vlan) - sizeof(click_ether), sizeof(ether->ether_shost) + sizeof(ether->ether_dhost));
    
    // set 802.1Q (VLAN) type
    vlanh->ether_vlan_proto = htons(ETHERTYPE_8021Q);
    
    // zero all VLAN values
    memset(&vlanh->ether_vlan_tci, '\0', sizeof(vlanh->ether_vlan_tci));
    
    q->set_ether_header((click_ether *) vlanh);
  	
  	return q;
  	
  } else
    return NULL;
}

void
VLANEncap::push(int, Packet *p)
{
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
VLANEncap::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

EXPORT_ELEMENT(VLANEncap)
CLICK_ENDDECLS
