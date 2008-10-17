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
#include "restorevlan.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>

#include "vlantable.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS

RestoreVLAN::RestoreVLAN()
: _table(NULL)
{
}

RestoreVLAN::~RestoreVLAN()
{
}

int
RestoreVLAN::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
            cpElement, "VLANTable", &_table,
		  cpEnd) < 0)
    return -1;
  
  if (_table->cast("VLANTable") == NULL)
  	return errh->error("Didn't specify an element of type VLANTable");
  
  return 0;
}

Packet *
RestoreVLAN::smaction(Packet *p)
{
  click_ether_vlan * vlanh = (click_ether_vlan *) p->ether_header();
  
  if (vlanh == NULL) {
  	//click_chatter("%{element}: WARN No ether header found.", this);
  	return NULL;
  }
  
  if (vlanh->ether_vlan_proto != htons(ETHERTYPE_8021Q)) {
  	//click_chatter("%{element}: WARN No vlan header found.", this);
  	return NULL;
  }
  
  if (WritablePacket *q = p->uniqueify()) {
    
    EtherAddress dst(vlanh->ether_dhost);
    
    uint16_t vid = _table->_vlans.find(dst, 0xFFFF);
    
    if (dst.is_broadcast()) {
      //click_chatter("%{element}: WARN dst is broadcast.", this);
      return p;
    }
      
    
    if (vid != 0xFFFF) {
	  // set VID
  	  vlanh->ether_vlan_tci = htons((vid & 0x0FFF) | (ntohs(vlanh->ether_vlan_tci) & 0xF000));
      //click_chatter("%{element}: INFO Setting vid to %u.", this, vid);
      return q;
		} else {
      //click_chatter("%{element}: WARN No vid found.", this);
      return q; 
    }
  } else
    return NULL;
}

void
RestoreVLAN::push(int port, Packet *p)
{
  (void) port;
  
  if (Packet *q = smaction(p))
    output(0).push(q);
  else
    checked_output_push(1, p);
}

Packet *
RestoreVLAN::pull(int port)
{
  (void) port;
	Packet *p;
  
  if ((p = input(0).pull()) != NULL) {
    Packet *q = smaction(p);
    if (q != NULL)
      return q;
    else {
      checked_output_push(1, p);
      return NULL;
    }
  }
  
  return p;
}

EXPORT_ELEMENT(RestoreVLAN)
CLICK_ENDDECLS
