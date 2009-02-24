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
#include "brnvlan.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/timer.hh>

// to generate dht packets
#include <elements/brn/dht/dhtcommunication.hh>

CLICK_DECLS

BRNVLAN::BRNVLAN() : 
    _my_ssid(""),
    _timer(this),
    _interval(DEFAULT_INTERVAL)
{
}

BRNVLAN::~BRNVLAN()
{
}

int
BRNVLAN::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SSID", cpkP+cpkM, cpString, /*"ssid",*/ &_my_ssid,
      "INTERVAL", cpkP+cpkM, cpSeconds, /*"time (s) between to updates of known vlans",*/ &_interval,
                 cpEnd) < 0)
        return -1;

  return 0;
}

int
BRNVLAN::initialize(ErrorHandler *errh)
{
    if (!add_vlan(_my_ssid, 0, false))
        return errh->error("Could not initialize with my SSID %s", _my_ssid.c_str());
  
    _timer.initialize(this);
    _timer.schedule_after_sec(_interval);
  
    return 0;
}


void
BRNVLAN::push(int, Packet *p)
{
  handle_dht_response(p);
  p->kill();
  return;
}


String
BRNVLAN::remove_end_bytes(String ssid) {
  return ssid.substring(0, ssid.find_right('@'));
}

// ssid: vlan (without @foo)
bool
BRNVLAN::add_vlan(String ssid, uint16_t vid, bool protect) {
  if (_vids.insert(ssid, vid) == false) {
    // vlan already known
    return false;
  }
  _vlans.insert(vid, VLAN(vid, ssid, protect));
  
  BRN_INFO("Added VLAN with ssid %s and vid %u.", ssid.c_str(), vid);
  
  return true;	
}

// ssid: vlan (without @foo)
bool
BRNVLAN::remove_vlan(String ssid) {

  uint16_t vid = _vids.find(ssid, 0xFFFF);
  if (vid != 0xFFFF) {
    _vids.remove(ssid);
    _vlans.remove(vid);
    
    BRN_INFO("Removed VLAN with ssid %s and vid %u.", ssid.c_str(), vid);
    return true; 
  }
  else {
    if (vid == 0xFFFF) {
      BRN_WARN("VLAN %s unknown.", ssid.c_str()); 
    }
  }
  
	return false;
}

bool
BRNVLAN::is_vlan(String ssid) {
  uint16_t vid = get_vlan(ssid);
  
  BRN_DEBUG("is_vlan() called with SSID %s and vid %u", ssid.c_str(), vid);
  
  return vid != 0xFFFF ? true : false;
}

bool
BRNVLAN::is_vlan(uint16_t vid) {
  return get_vlan(vid) != NULL ? true : false;
}

uint16_t
BRNVLAN::get_vlan(String ssid) {
  String vlan(_my_ssid);
  if (vlan != ssid) {
    // not a valid vlan name
    String prefix = ssid.substring(ssid.find_right('@') + 1, ssid.length() - ssid.find_right('@') - 1);
    BRN_DEBUG("Testing prefix for %s; Computed prefix \"%s\".", ssid.c_str(), prefix.c_str());
    if (prefix != _my_ssid)
      return 0xFFFF;
  
    vlan = remove_end_bytes(ssid);
  }
  
  uint16_t vid = _vids.find(vlan, 0xFFFF);
  
  // not found => look in dht
  if (vid == 0xFFFF)
    get_vlan_from_dht(vlan);

  return vid;
}

BRNVLAN::VLAN*
BRNVLAN::get_vlan(uint16_t vid) {
  VLAN* vlan = _vlans.findp(vid);

  // look also in dht
  if (vid >= 3 && vid < 0xFFFF && vlan == NULL)
    get_vlan_from_dht(vid);

  return vlan;
}

const BRNVLAN::VLANMembers*
BRNVLAN::get_clients_from_vid(uint16_t vid) {
  assert(is_vlan(vid) == true);
  return get_vlan(vid)->get_members();
}


// timer to update values
// for vlans, which are in use
void
BRNVLAN::run_timer(Timer* t) {
  BRN_INFO("Executing timer to check, if all vlans are still there.");
  
  for (VLANConstIter i = _vlans.begin(); i.live(); i++) {
    VLAN vlan = i.value();
    if ((vlan._vid < 0x0FFF) && (vlan._vid >= 3)) {
      BRN_INFO(" Checking vlan %s with vid %u.", vlan._name.c_str(), vlan._vid);
      get_vlan_from_dht(vlan._name);
    }
  }
  
  t->reschedule_after_sec(_interval);
}

uint16_t
BRNVLAN::random_vid() {
  uint16_t vid = random() & 0x0FFF;
  vid |= 0x0003;
  return vid;
}

/**
 * 
 * Methods to communicate with DHT
 * 
 */
void
BRNVLAN::get_vlan_from_dht(String ssid) {
  BRN_INFO("Generating packet to look for %s in DHT", ssid.c_str());

  WritablePacket *dht_p_out = DHTPacketUtil::new_dht_packet();
  _unique_id = ( ( _unique_id - ( _unique_id % DHT_VLAN_NUM ) ) + DHT_VLAN_GET_VLAN + DHT_VLAN_NUM) % 255;
    
  BRN_CHECK_EXPR_RETURN(_unique_id % DHT_VLAN_NUM != 0,
    ("Unique ID %u mod %u != %u", _unique_id, DHT_VLAN_NUM, DHT_VLAN_GET_VLAN), return;);
    
  DHTPacketUtil::set_header(dht_p_out, VLANS, DHT, 0, READ, _unique_id);
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_VLAN_NAME, strlen(ssid.c_str()) + 1, (uint8_t *) ssid.c_str());
    
  output(0).push(dht_p_out);
}

void
BRNVLAN::get_vlan_from_dht(uint16_t vid) {
  assert((vid < 0x0FFF) && (vid >= 3));
  
  BRN_INFO("Generating packet to look for vid %u in DHT", vid);

  WritablePacket *dht_p_out = DHTPacketUtil::new_dht_packet();
      
  _unique_id = ( ( _unique_id - ( _unique_id % DHT_VLAN_NUM ) ) + DHT_VLAN_GET_VID + DHT_VLAN_NUM ) % 255;
    
  BRN_CHECK_EXPR_RETURN(_unique_id % DHT_VLAN_NUM != DHT_VLAN_GET_VID,
    ("Unique ID %u mod DHT_VLAN_NUM != DHT_VLAN_GET_VID", _unique_id, DHT_VLAN_NUM, DHT_VLAN_GET_VID), return;);
        
  DHTPacketUtil::set_header(dht_p_out, VLANS, DHT, 0, READ, _unique_id);
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_VLAN_ID, sizeof(vid), (uint8_t *) &vid);
    
  output(0).push(dht_p_out);
}

void
BRNVLAN::add_vlan_to_dht(String ssid, uint16_t vid) {
  assert((vid < 0x0FFF) && (vid >= 3));
  
  WritablePacket *dht_vlan_string = DHTPacketUtil::new_dht_packet();
    
  _unique_id = ( ( _unique_id - ( _unique_id % DHT_VLAN_NUM ) ) + DHT_VLAN_ADD_VLAN + DHT_VLAN_NUM ) % 255;
  
  BRN_CHECK_EXPR_RETURN(_unique_id % DHT_VLAN_NUM != DHT_VLAN_ADD_VLAN,
      ("Unique ID %u mod DHT_VLAN_NUM != DHT_VLAN_ADD_VLAN", _unique_id, DHT_VLAN_NUM, DHT_VLAN_ADD_VLAN), return;);
  
  DHTPacketUtil::set_header(dht_vlan_string, VLANS, DHT, 0, INSERT, _unique_id);
  dht_vlan_string = DHTPacketUtil::add_payload(dht_vlan_string, 0, TYPE_VLAN_NAME, strlen(ssid.c_str()) + 1, (uint8_t *) ssid.c_str());
  dht_vlan_string = DHTPacketUtil::add_payload(dht_vlan_string, 1, TYPE_VLAN_ID, sizeof(vid), (uint8_t *) &vid);
  
  BRN_INFO("Generating packet to add vlan %s with vid %u in DHT with unique %u", ssid.c_str(), vid, _unique_id);
  
  output(0).push(dht_vlan_string);
}

void
BRNVLAN::add_vid_to_dht(String ssid, uint16_t vid) {
  assert((vid < 0x0FFF) && (vid >= 3));
  
  WritablePacket *dht_vlan_id = DHTPacketUtil::new_dht_packet();
  _unique_id = ( ( _unique_id - ( _unique_id % DHT_VLAN_NUM ) ) + DHT_VLAN_ADD_VID + DHT_VLAN_NUM ) % 255;
  
  BRN_CHECK_EXPR_RETURN(_unique_id % DHT_VLAN_NUM != DHT_VLAN_ADD_VID,
      ("Unique ID %u mod %u != %", _unique_id, DHT_VLAN_NUM, DHT_VLAN_ADD_VID), return;);
  
  DHTPacketUtil::set_header(dht_vlan_id, VLANS, DHT, 0, INSERT, _unique_id);
  dht_vlan_id = DHTPacketUtil::add_payload(dht_vlan_id, 0, TYPE_VLAN_ID, sizeof(vid), (uint8_t *) &vid);
  dht_vlan_id = DHTPacketUtil::add_payload(dht_vlan_id, 1, TYPE_VLAN_NAME, strlen(ssid.c_str()) + 1, (uint8_t *) ssid.c_str());

  BRN_INFO("Generating packet to add vlan %s with vid %u in DHT with unique %u", ssid.c_str(), vid, _unique_id);

  output(0).push(dht_vlan_id);
}

void
BRNVLAN::remove_vlan_from_dht(String ssid, uint16_t vid) {
  assert((vid < 0x0FFF) && (vid >= 3));
  
  BRN_INFO("Generating packet to remove vlan %s with vid %u in DHT", ssid.c_str(), vid);
  
  WritablePacket *dht_vlan_string = DHTPacketUtil::new_dht_packet();
  _unique_id = ( ( _unique_id - ( _unique_id % DHT_VLAN_NUM ) ) + DHT_VLAN_REMOVE_VLAN + DHT_VLAN_NUM ) % 255;

  BRN_CHECK_EXPR_RETURN(_unique_id % DHT_VLAN_NUM != DHT_VLAN_REMOVE_VLAN,
    ("Unique ID %u mod %u != DHT_VLAN_REMOVE_VLAN", _unique_id, DHT_VLAN_NUM, DHT_VLAN_REMOVE_VLAN), return;);
        
  DHTPacketUtil::set_header(dht_vlan_string, VLANS, DHT, 0, REMOVE, _unique_id );
  dht_vlan_string = DHTPacketUtil::add_payload(dht_vlan_string, 0, TYPE_VLAN_NAME, strlen(ssid.c_str()) + 1, (uint8_t *) ssid.c_str());
    
  output(0).push(dht_vlan_string);
}

void
BRNVLAN::remove_vid_from_dht(String ssid, uint16_t vid) {
  assert((vid < 0x0FFF) && (vid >= 3));
  
  BRN_INFO("Generating packet to remove vlan %s with vid %u in DHT", ssid.c_str(), vid);
  
  WritablePacket *dht_vlan_id = DHTPacketUtil::new_dht_packet();
  _unique_id = ( ( _unique_id - ( _unique_id % DHT_VLAN_NUM ) ) + DHT_VLAN_REMOVE_VID + DHT_VLAN_NUM ) % 255;
  
  BRN_CHECK_EXPR_RETURN(_unique_id % DHT_VLAN_NUM != DHT_VLAN_REMOVE_VID,
      ("Unique ID %u mod %u != %u", _unique_id, DHT_VLAN_NUM, DHT_VLAN_REMOVE_VID), return;);
  
  DHTPacketUtil::set_header(dht_vlan_id, VLANS, DHT, 0, REMOVE, _unique_id);
  dht_vlan_id = DHTPacketUtil::add_payload(dht_vlan_id, 0, TYPE_VLAN_ID, sizeof(vid), (uint8_t *) &vid);

  output(0).push(dht_vlan_id);
}


String
BRNVLAN::extract_vlan(Packet *p, int at_number) {
  struct dht_packet_payload *vlan_entry;
  vlan_entry = DHTPacketUtil::get_payload_by_number(p, at_number);
  
  String vlan((char *) vlan_entry->data);
  BRN_DEBUG("GOT VLAN %s", vlan.c_str());
  
  return vlan;
}

uint16_t
BRNVLAN::extract_vid(Packet *p, int at_number) {
  struct dht_packet_payload *vid_entry;
  vid_entry = DHTPacketUtil::get_payload_by_number(p, at_number);
  
  if (vid_entry == NULL) {
    BRN_ERROR(" vid_emtry == NULL ");
    return 0xFFFF; 
  }
  
  uint16_t vid;
  memcpy(&vid, vid_entry->data, vid_entry->len);
  BRN_DEBUG("GOT VID %u", vid);
  return vid;
}

void 
BRNVLAN::handle_dht_response(Packet* p) {

  struct dht_packet_header *dht_header = (struct dht_packet_header*)p->data();

  switch ( dht_header->id % DHT_VLAN_NUM ) {
    case DHT_VLAN_GET_VLAN: {
      // extract vid and ssid
      String ssid = extract_vlan(p, 0);
      uint16_t vid = extract_vid(p, 1);
      
      if ( dht_header->flags == 0 ) {
        BRN_INFO("GET_VLAN OK");
        // add vlan
        add_vlan(ssid, vid, false);
      }
      else {
        BRN_WARN("GET_VLAN FAILED");
        remove_vlan(ssid);
      }
       
      break;
    }
    case DHT_VLAN_GET_VID: {
      // extract vid and ssid
      String ssid = extract_vlan(p, 1);
      uint16_t vid = extract_vid(p, 0);
      if ( dht_header->flags == 0 ) {
        BRN_INFO("GET_VID OK");
        // add vlan
        add_vlan(ssid, vid, false);    
      }
      else {
        BRN_ERROR("GET_VID FAILED");
        remove_vlan(ssid);
      }
       
      break;
    }
    case DHT_VLAN_ADD_VLAN: {
      // extract vid and ssid
      String ssid = extract_vlan(p, 0);
      uint16_t vid = extract_vid(p, 1);
      
      if ( dht_header->flags == 0 ) {
        BRN_INFO("ADD_VLAN OK");
         
        // Adding vlan successful
        BRN_INFO("Adding VLAN %s with vid %u in DHT successfull.", ssid.c_str(), vid);
         
        // TODO
        // fails, if vlan is already known as local vlan
        add_vlan(ssid, vid, false);
      }
      else {
        BRN_WARN("ADD_VLAN FAILED");
          
        // rollback
        // VLAN already known
        remove_vid_from_dht(ssid, vid);
        BRN_WARN("Removed vid %u for ssid %s.", vid, ssid.c_str());
      }
      
      break;
    }
    case DHT_VLAN_ADD_VID: {
      // extract vid and ssid
      String ssid = extract_vlan(p, 1);
      uint16_t vid = extract_vid(p, 0);
       
      if ( dht_header->flags == 0 ) {
        BRN_INFO("ADD_VID OK");
         
        // Adding vlan successful
        BRN_INFO("Adding VID for VLAN %s with vid %u in DHT successfull.", ssid.c_str(), vid);
         
        // and add the vlan name
        add_vlan_to_dht(ssid, vid);
      }
      else {
        BRN_WARN("ADD_VID FAILED");
          
        // vid already in use
        // try again
        add_vid_to_dht(ssid, random_vid());
          
        BRN_INFO("Trying again VID for VLAN %s with vid %u in DHT successfull.", ssid.c_str(), vid);
      }
      
      break;
    }
    case DHT_VLAN_REMOVE_VLAN: {
      if ( dht_header->flags == 0 ) {
        BRN_INFO("REMOVE_VLAN OK");
        
        // extract vid and ssid
        String ssid = extract_vlan(p, 0);

        //
        remove_vlan(ssid);
      }
      else {
        BRN_ERROR("REMOVE_VLAN FAILED");
      }

      break;
    }
    case DHT_VLAN_REMOVE_VID: {
      if ( dht_header->flags == 0 ) {
        BRN_INFO("REMOVE_VID OK");
      }
      else {
        BRN_ERROR("REMOVE_VID FAILED");
      }
      break;
     }
     default: {
        BRN_ERROR("Strange DHT ID %d. Fix me.", dht_header->id);
     }
  }
}

/**
 * 
 * Handlers
 * 
 */

enum { HANDLER_GET_VLAN, HANDLER_GET_VLANS, HANDLER_ADD_VLAN, HANDLER_REMOVE_VLAN, HANDLER_ADD_LOCAL_VLAN, HANDLER_REMOVE_LOCAL_VLAN };

String
BRNVLAN::read_handler(Element *e, void *thunk) {
    BRNVLAN *bv = static_cast<BRNVLAN *> (e);
    
    switch ((uintptr_t) thunk) {
    	case HANDLER_GET_VLANS: {
        	StringAccum sa;
        	
        	/**
        	 * 
        	 * @todo
        	 * Impove printing output
        	 * 
        	 */
        	sa << "SSID\tVLAN ID\tClients\n";
        
        	// iterate over all vlans
          
          
        	for (VLANConstIter i = bv->_vlans.begin(); i.live(); i++) {
            	VLAN vlan = i.value();
              assert(i.key() == vlan._vid);
              
            	sa << vlan._name;

              if (vlan._name != bv->_my_ssid)
                sa <<  "@" << bv->_my_ssid;
              
              if (vlan.is_protected() == true)
                sa << " (wep)";
              
            	sa << "\t" << (uint32_t) vlan._vid;

              if (vlan._vid == 2)
                sa <<  " (local vlan)";

              VLANMembers *members = vlan.get_members();
              
              for (MembersConstIter l = members->begin(); l.live(); l++) {
                sa  << " " << l.key().unparse();  
              } 
              
              sa << "\n";
        	}
          

        return sa.take_string();
        }
    	default:
        	return "<error>\n";
    }
}

int
BRNVLAN::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh) {
    BRNVLAN *bv = static_cast<BRNVLAN *> (e);
    String s = cp_uncomment(data);
    
    switch ((intptr_t) thunk) {
    case HANDLER_GET_VLAN: {
    	
    	uint16_t vlanid = bv->get_vlan(s + "@" + bv->_my_ssid);
    	if (vlanid == 0xFFFF)
        return errh->error("No VLAN for SSID %s is known. DHT lookup was started.", s.c_str());

      return 0;
    	
    	break;
    }
    case HANDLER_ADD_VLAN: {
        
        /**
         * 
         * @todo
         * How to generate an VLAN ID
         * 
         */
        String name;
        bool protect;
        if (cp_va_space_parse(s, bv, errh,
                              cpString, "VLAN name", &name,
                              cpBool, "Protected?", &protect,
                              cpEnd) < 0)
            return errh->error("wrong arguments to handler 'add_vlan'");
        
        uint16_t vid = bv->random_vid();
        bv->add_vid_to_dht(name, vid);
         
        errh->warning("Trying to add VLAN for SSID %s and vid %u.", name.c_str(), vid);
        return 0;
        
    	break;
    }
    case HANDLER_REMOVE_VLAN: {
        
        //if (!bv->remove_vlan(s))
        //        return errh->error("Removing VLAN for SSID %s failed.", s.c_str());
        uint16_t vid = bv->get_vlan(s + "@" + bv->_my_ssid);
        
        if (vid >= 3 && vid <= 0x0FFF) {
          bv->remove_vlan_from_dht(s, vid);
          bv->remove_vid_from_dht(s, vid);
        }
        else {
          return errh->error("Removing VLAN for SSID %s failed.", s.c_str());
        }
        
        return 0;
    	break;        

    }
    case HANDLER_ADD_LOCAL_VLAN: {
        
        /**
         * 
         * @todo
         * How to generate an VLAN ID
         * 
         */
        String name;
        bool protect;
        if (cp_va_space_parse(s, bv, errh,
                              cpString, "VLAN name", &name,
                              cpBool, "Protected?", &protect,
                              cpEnd) < 0)
            return errh->error("wrong arguments to handler 'add_local_vlan'");
        
         
        if (!bv->add_vlan(name, 2, protect))
                return errh->error("Adding VLAN for SSID %s failed.", s.c_str());
        return 0;
        
      break;
    }
    case HANDLER_REMOVE_LOCAL_VLAN: {
        
        if (!bv->remove_vlan(s))
                return errh->error("Removing VLAN for SSID %s failed.", s.c_str());

        return 0;
      break; 
    }
    default:
        return errh->error("internal error");
	}
	
	return 0;
}
    

void
BRNVLAN::add_handlers() {
    BRNElement::add_handlers();

    add_read_handler("get_vlans", read_handler, (void *) HANDLER_GET_VLANS);
    
    add_write_handler("get_vlan", write_handler, (void *) HANDLER_GET_VLAN);
    add_write_handler("add_vlan", write_handler, (void *) HANDLER_ADD_VLAN);
    add_write_handler("remove_vlan", write_handler, (void *) HANDLER_REMOVE_VLAN);
    
    add_write_handler("add_local_vlan", write_handler, (void *) HANDLER_ADD_LOCAL_VLAN);
    add_write_handler("remove_local_vlan", write_handler, (void *) HANDLER_REMOVE_LOCAL_VLAN);
}


void
BRNVLAN::VLAN::remove_member(EtherAddress client) {
    _members.remove(client);
}

void
BRNVLAN::VLAN::add_member(EtherAddress client) {
    _members.insert(client, true);
}



EXPORT_ELEMENT(BRNVLAN)
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, int>;
#endif
#include <click/vector.cc>
CLICK_ENDDECLS
