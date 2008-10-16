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
#include <click/etheraddress.hh>
#include <click/error.hh>
#include <click/timer.hh>
#include <elements/brn/wifi/ap/assoclist.hh>
#include <elements/brn/brn.h>
#include "brncheckvlan.hh"
#include "brnvlan.hh"

CLICK_DECLS


BRNCheckVLAN::BRNCheckVLAN() :
    _me(EtherAddress())
{
}

BRNCheckVLAN::~BRNCheckVLAN()
{
}

int
BRNCheckVLAN::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
  				  cpOptional,
                  cpElement, "AssocList", &_assoc_list,
                  cpElement, "BRNVLAN", &_brn_vlans,
                  cpEtherAddress, "EtherAddress", &_me,
                  cpKeywords,
                  "ASSOCLIST", cpElement, "AssocList", &_assoc_list,
                  "BRNVLAN", cpElement, "BRNVLAN", &_brn_vlans,
                  "ETHERADDRESS", cpEtherAddress, "EtherAddress", &_me,
          cpEnd) < 0)
    return -1;
  
  // errors
  if (_assoc_list->cast("AssocList") == NULL)
    return errh->error("specified no AssocList element");

  if (_brn_vlans->cast("BRNVLAN") == NULL)
    return errh->error("specified no BRNVLAN element");

  if (!_me)
    return errh->error("No ether address specified");

  return 0;
}


Packet *
BRNCheckVLAN::smaction(Packet *p) {
  click_ether_vlan * vlanh = (click_ether_vlan *) p->data();

  BRN_CHECK_EXPR_RETURN(vlanh == NULL,
              ("Ether header not available. Killing packet."), p->kill(); return NULL;);
  
  BRN_CHECK_EXPR_RETURN(ntohs(vlanh->ether_vlan_proto) == ETHERTYPE_BRN,
              ("Got BRN packet. Error in click configuration."), return p;);

  if (ntohs(vlanh->ether_vlan_proto) != ETHERTYPE_8021Q) 
  {
  	BRN_DEBUG(" vlanh->ether_vlan_encap_proto == 0x%x", ntohs(vlanh->ether_vlan_encap_proto));
  	BRN_DEBUG(" vlanh->ether_vlan_proto == 0x%x and ETHERTYPE_8021Q = 0x%x", ntohs(vlanh->ether_vlan_proto), ETHERTYPE_8021Q);
  	BRN_INFO("No vlan header found, but we pass the packet. Is this policy correct?");
  	return p;
  }


  EtherAddress dst(vlanh->ether_dhost);
  uint16_t vlanid = ntohs(vlanh->ether_vlan_tci) & 0x0FFF;
  
  String dst_ssid = _assoc_list->get_ssid(dst);
  uint16_t dst_vid = _brn_vlans->get_vlan(dst_ssid);

  BRN_INFO("Getting packet to %s (%s equals vid %u) with vid %u.", dst.s().c_str(), dst_ssid.c_str(), dst_vid, vlanid);

  // TODO
  if (dst == _me) {
      BRN_WARN("Received a packet for my WLAN address. Packet from virtual station? Fix me.");
      return p;
  }

  if (dst.is_broadcast()) {
      BRN_WARN("Received broadcast packet. Packet from virtual station? Fix me.");
      return p;
  }
  
  // check if dst is in same vlan as set by the packet or vid = 0
  if ((dst_vid != 0xFFFF && dst_vid == vlanid) || dst_vid == 0) {
  	BRN_INFO("Packet passed");
    return p;
  }
  else {
  	assert(dst_vid != vlanid);
  	
    BRN_WARN("Packet from VID %u to VID %u pushed via checked_output.", vlanid, dst_vid);
    checked_output_push(1, p);
  	return NULL;
  }
}


Packet *
BRNCheckVLAN::pull(int)
{
	Packet *p = input(0).pull();
	if (p != NULL)
		return smaction(p);
		
	return NULL;
}


void
BRNCheckVLAN::push(int, Packet *p)
{
    Packet *q = smaction(p);
    if (q != NULL)
	output(0).push(smaction(p));
}

EXPORT_ELEMENT(BRNCheckVLAN)
CLICK_ENDDECLS
