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
#include "vlantag.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS

VLANTag::VLANTag()
: _vid(0xFFFF),
  _prio(0xFF)
{
}

VLANTag::~VLANTag()
{
}

int
VLANTag::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
  				  cpOptional,
                  cpUnsignedShort, "VLAN identifier", &_vid,
                  cpKeywords,
                  "VLANID", cpUnsignedShort, "VLAN identifier", &_vid,
                  "USER_PRIORITY", cpUnsignedShort, "User priority", &_prio,
                  //"CFI", cpBool, "Canonical Format Indicator", &_cfi,
		  cpEnd) < 0)
    return -1;
  
  // at least one value has to be specified
  if (_vid == 0xFFFF && _prio == 0xFF)
    //return errh->error("Neither VLANID, USER_PRIORITY nor CFI specified");
    return errh->error("Neither VLANID nor USER_PRIORITY specified");
  
  // errors
  if (_vid != 0xFFFF && _vid >= 0xFFF)
    return errh->error("specified VLAN identifier >= 0xFFF");

  if (_prio != 0xFF && _prio > 7)
    return errh->error("prio ranges from 0 to 7");
  
  
  // warnings
  if (_vid == 0)
  	errh->warning("A value of 0 indicates that frames do not belong to any VLAN group.\nThis value is for using the user priority field only.");
  
  if (_vid == 1)
  	errh->warning("A value of 1 indicates that frames belong to the default VLAN group");
  
  //if (_cfi)
  //	errh->warning("CFI bit set. Strange. Are you using Token-Ring?");
  
  return 0;
}

Packet *
VLANTag::smaction(Packet *p)
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
  	//click_chatter("ERROR No vlan header found. Killing packet. ");
  	p->kill();
  	return NULL;
  	
  }
  
  if (WritablePacket *q = p->uniqueify())
  {
		if (_vid != 0xFFFF) {
			// set VID
	  		vlanh->ether_vlan_tci = htons((_vid & 0x0FFF) | (ntohs(vlanh->ether_vlan_tci) & 0xF000));
		}
  	
  	if (_prio != 0xFF) {
  		// set PRIORITY
  		vlanh->ether_vlan_tci = htons(((_prio << 13) & 0xE000) | (ntohs(vlanh->ether_vlan_tci) & 0x1FFF));
  	}
  	
  	return q;
  	
  } else
    return NULL;
}

void
VLANTag::push(int port, Packet *p)
{
  (void) port;
  
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
VLANTag::pull(int port)
{
  (void) port;
	
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return NULL;
}

/**
 * 
 * Handlers
 * 
 */

enum { HANDLER_GET_VLANID, HANDLER_GET_USER_PRIORITY, HANDLER_SET_VLANID, HANDLER_SET_USER_PRIORITY };

String
VLANTag::read_handler(Element *e, void *thunk) {
    VLANTag *vtag = static_cast<VLANTag *> (e);
    switch ((uintptr_t) thunk) {
    	case HANDLER_GET_VLANID: {
        	return String(vtag->_vid);
    	}
    	case HANDLER_GET_USER_PRIORITY: {
        	return String(vtag->_prio);
    	}
    	default:
        	return "<error>\n";
    }
}

int
VLANTag::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh) {
    VLANTag *vtag = static_cast<VLANTag *> (e);
    String s = cp_uncomment(data);
    
    switch ((intptr_t) thunk) {
    case HANDLER_SET_VLANID: {
    	unsigned vlanid;
    	if (!cp_unsigned(s, &vlanid)) 
      		return errh->error("vland id must be unsigned");
    	
    	if (vlanid >= 0xFFF)
    		return errh->error("specified VLAN identifier >= 0xFFF");
    
    	if (vlanid == 0)
  			errh->warning("A value of 0 indicates that frames do not belong to any VLAN group.\nThis value is for using the user priority field only.");
  
  		if (vlanid == 1)
  			errh->warning("A value of 1 indicates that frames belong to the default VLAN group");	
    	
    	vtag->_vid = vlanid;
    	break;
    }
    case HANDLER_SET_USER_PRIORITY: {
        
        unsigned prio;
    	
    	if (!cp_unsigned(s, &prio)) 
      		return errh->error("user priority id must be unsigned");
    	
    	if (prio > 7)
    		return errh->error("prio ranges from 0 to 7");
  
    	vtag->_prio = prio;
    	break;
    }
    default:
        return errh->error("internal error");
    }
    return (0);
}

void
VLANTag::add_handlers() {
	
    add_read_handler("get_vlanid", read_handler, (void *) HANDLER_GET_VLANID);
    add_read_handler("get_user_priority", read_handler, (void *) HANDLER_GET_USER_PRIORITY);

    add_write_handler("set_vlanid", write_handler, (void *) HANDLER_SET_VLANID);
    add_write_handler("set_user_priority", write_handler, (void *) HANDLER_SET_USER_PRIORITY);
}

EXPORT_ELEMENT(VLANTag)
CLICK_ENDDECLS
