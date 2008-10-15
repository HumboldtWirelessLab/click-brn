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
#include "checkvlan.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/ether.h>

CLICK_DECLS


CheckVLAN::CheckVLAN()
{
}

CheckVLAN::~CheckVLAN()
{
}

int
CheckVLAN::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() != noutputs())
    return errh->error("need %d arguments, one per output port", noutputs());
  
  unsigned int vlan_id;
  
  // last argument a '-'
  if (conf[conf.size() - 1] != "-" && !cp_unsigned(conf[conf.size() - 1], &vlan_id)) {
  	return errh->error("Last argument is neither a integer nor a '-'");
  }
  
  int length = conf.size();
  
  if (conf[conf.size() - 1] == "-") {
  	length = conf.size() - 1;
  }
  
  for (int i = 0; i < length; i++) {
  	
  	if (!cp_unsigned(conf[i], &vlan_id)) {
  		return errh->error("%s is no integer", ((String) conf[i]).c_str());	
  	}
  	
  	if (vlan_id >= 0xFFF)
    	return errh->error("specified VLAN identifier >= 0xFFF");
  	
  	if(!_vlan_ids.insert((uint16_t) vlan_id, i))
  		return errh->error("VLAN ID %u specified at position %d was already given before. Only one is allowed.", (uint16_t) vlan_id, i);
  }

  return 0;
}


void
CheckVLAN::add_handlers()
{
}


void
CheckVLAN::push(int, Packet *p)
{
  click_ether_vlan * vlanh = (click_ether_vlan *) p->ether_header();
  
  if (vlanh == NULL) {
  	//click_chatter("ERROR No ether header found. Killing packet. ");
  	p->kill();
  	return;
  }
  
  if (ntohs(vlanh->ether_vlan_proto) != ETHERTYPE_8021Q) {
    //click_chatter("%{element} WARN No vlan header found. Killing packet. vlanh->ether_vlan_proto = %u.", this, ntohs(vlanh->ether_vlan_proto));
    p->kill();
    return;
  }
  
  uint16_t vlanid = ntohs(vlanh->ether_vlan_tci) & 0x0FFF;
  
  // find matching output port
  uint16_t port = _vlan_ids.find(vlanid, _vlan_ids.size());
  if (port == _vlan_ids.size()) {
    // don't know that vlan
    
    //click_chatter("%{element} No port found, pushing it to port %u, if available", this, port);
  	checked_output_push(port, p);
    return;
  }
  else {
    //click_chatter("%{element} Pushing packet to matching port %u", this, port);
    output(port).push(p);
    return;
  }

  //click_chatter("%{element} Killing packet.", this);
  p->kill();
  return;
}

EXPORT_ELEMENT(CheckVLAN)
#include <click/bighashmap.cc>
CLICK_ENDDECLS
