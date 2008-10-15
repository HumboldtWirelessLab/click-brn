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
#include "storevlan.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>

#include "vlantable.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS


StoreVLAN::StoreVLAN()
{
}

StoreVLAN::~StoreVLAN()
{
}

int
StoreVLAN::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
            cpElement, "VLANTable", &_table,
      cpEnd) < 0)
    return -1;
  
  if (_table->cast("VLANTable") == NULL)
    return errh->error("Didn't specify an element of type VLANTable");
  
  return 0;
}


void
StoreVLAN::push(int, Packet *p)
{
  click_ether_vlan * vlanh = (click_ether_vlan *) p->ether_header();
  
  if (vlanh == NULL) {
  	//click_chatter("%{element}: WARN No ether header found.", this);
  	checked_output_push(1, p);
  	return;
  }
  
  if (vlanh->ether_vlan_proto != htons(ETHERTYPE_8021Q)) {
  	//click_chatter("%{element}: WARN No vlan header found.", this);
  	output(0).push(p);
  	return;
  }
  
  EtherAddress src(vlanh->ether_shost);
  uint16_t vlanid = ntohs(vlanh->ether_vlan_tci) & 0x0FFF;
  
  
  uint16_t *vid = _table->_vlans.findp(src);
  if (vid == NULL) {
    _table->_vlans.insert(src, vlanid);
    //click_chatter("Storing src %s - VID %u", src.s().c_str(), vlanid);
  }
  else {
    // update
    *vid = vlanid;
    //click_chatter("Updating src %s - VID %u", src.s().c_str(), vlanid);
  }
  
  output(0).push(p);
  return;
}

EXPORT_ELEMENT(StoreVLAN)
#include <click/bighashmap.cc>
CLICK_ENDDECLS
