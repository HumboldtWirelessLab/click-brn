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
#include "brnvlantag.hh"
#include <click/confparse.hh>
#include <click/etheraddress.hh>
#include <elements/brn/wifi/ap/assoclist.hh>
#include "brnvlan.hh"

CLICK_DECLS

BRNVLANTag::BRNVLANTag() : 
    _assoc_list(NULL),
    _me(EtherAddress())
{
}

BRNVLANTag::~BRNVLANTag()
{
}

int
BRNVLANTag::configure(Vector<String> &conf, ErrorHandler *errh)
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
BRNVLANTag::smaction(Packet *p)
{
  click_ether_vlan * vlanh = (click_ether_vlan *) p->ether_header();

  BRN_CHECK_EXPR_RETURN(vlanh == NULL,
              ("Ether header not available. Killing packet."), p->kill(); return NULL;);

  if (ntohs(vlanh->ether_vlan_proto) != ETHERTYPE_8021Q)
  {
        BRN_WARN("No vlan header found, but we pass the packet. Is this policy correct?");
        return p;
  }
  
  if (WritablePacket *q = p->uniqueify())
  {
  	
  	EtherAddress src(vlanh->ether_shost);  	

  	// lookup stations SSID
  	AssocList::ClientInfo *cinfo = _assoc_list->get_entry(src);
  	if (cinfo == NULL)
  	{
          if (src == _me) {
  		      BRN_WARN("Station %s is an virtual station. Check policies anyway.", src.unparse().c_str());
            return p;
          }

          // TODO
  		  BRN_WARN("We got a packet for the unknown (not in assoc list) client %s. The source may be the service MAC. Pass it, but fix me.", src.unparse().c_str());
  		  p->kill();
  		  return NULL;
  	}
  	
  	String ssid(cinfo->get_ssid());
  	
  	if (!(cinfo->get_state() == AssocList::ASSOCIATED && ssid != "")) {
    	BRN_ERROR("Client %s is not associated and its ssid is %s. But a packet from it must be tagged. Maybe a packet from ARP.", src.unparse().c_str(), ssid.c_str());
    }
  	
  	// and find matching VID
  	uint16_t vid = _brn_vlans->get_vlan(ssid);
  	
  	// set matching VID
  	if (vid != 0xFFFF) {
		// set VID
  		vlanh->ether_vlan_tci = htons((vid & 0x0FFF) | (ntohs(vlanh->ether_vlan_tci) & 0xF000));
	
		BRN_INFO("Setting vid %u on packet from %s with SSID %s", vid, src.unparse().c_str(), ssid.c_str());
	}
	else {
		BRN_WARN("VLAN for SSID %s not known, but requested.", ssid.c_str());	
	}
  	return q;
  	
  } else
    return NULL;
}

void
BRNVLANTag::push(int, Packet *p)
{
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
BRNVLANTag::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

EXPORT_ELEMENT(BRNVLANTag)
CLICK_ENDDECLS
