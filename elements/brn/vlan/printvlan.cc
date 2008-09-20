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
#include "printvlan.hh"
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

CLICK_DECLS

PrintVLAN::PrintVLAN()
{
}

PrintVLAN::~PrintVLAN()
{
}

int
PrintVLAN::configure(Vector<String> &, ErrorHandler*)
{
  return 0;
}

Packet *
PrintVLAN::simple_action(Packet *p)
{
	
	click_ether_vlan * vlanh = (click_ether_vlan *) p->ether_header();
	
	if (vlanh == NULL)
  	{
  		//click_chatter("ERROR No ether header found. Killing packet. ");
  		p->kill();
  		return NULL;
  	}
  
  	if (vlanh->ether_vlan_proto != htons(ETHERTYPE_8021Q))
  	{
  		//click_chatter("ERROR No vlan header found. Pass through.");
  		return p;
  	}
		
	
	StringAccum sa(60);
	
	if (sa.out_of_memory()) {
		//click_chatter("no memory for PrintVLAN");
		return p;
  }
	
	uint16_t tci = ntohs(vlanh->ether_vlan_tci);
	
	sa << "Packet has "
	   << "VLAN ID " << (tci & 0x0FFF)	
	   << " and user priority " << ((tci & 0xE000) >> 13)
	   << " and CFI is " << (bool) ((tci & 0x1000) >> 12);
	   
	click_chatter("%s", sa.c_str());
	
	return p;
	
}

EXPORT_ELEMENT(PrintVLAN)
CLICK_ENDDECLS
