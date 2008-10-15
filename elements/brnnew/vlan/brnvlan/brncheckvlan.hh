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

#ifndef CLICK_BRNCHECKVLAN_HH
#define CLICK_BRNCHECKVLAN_HH
#include <elements/brn/brnelement.hh>

CLICK_DECLS

class EtherAddress;
class AssocList;
class BRNVLAN;

/**
 * This element checks, if a VLAN's frame destination is part of the frame's VLAN ID.
 * 
 * The element examines the destination address and the VLAN ID from the vlan frame. If
 * the destination's VLAN ID equals the frame's VLAN ID, the packet is outputed on
 * port 0, else it is outputed on port 1 or killed, if that port isn't available.   
 * 
 * 
 * Usage:
 * <pre>
 * ...
 * -> check_vlan :: BRNCheckVLAN(ASSOCLIST an_assoc_list, BRNVLAN an_brn_vlan) // or BRNVLANTag(an_assoc_list, an_brn_vlan)
 * -> Print("The destination is part of the VLAN specified by the frame's VLAN ID")
 * ...
 * 
 * check_vlan[1]
 * -> Print("The destination isn't a part of the frame's VLAN")
 * ...
 * </pre>
 * 
 * <h3>In- and Output Ports</h3>
 * 
 * <pre>
 * input[0]        802.1Q
 * [0]output       802.1Q
 * [1]output       802.1Q
 * </pre> 
 * @note
 * Incoming frames not matching the above specification are killed with an error message.
 * 
 * 
 * @note
 * To find the VLAN ID of a station, we use the element BRNVLAN, which stores for each
 * SSID a VLAN ID. The stations SSID is examined through the element AssocList, which stores
 * the SSID a station is associated with.
 *
 * <hr>
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-03 13:09:24 +0000 (Tue, 03 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2894 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/brncheckvlan.hh $
 * @version 0.5
 * @since 0.5
 * 
 */

class BRNCheckVLAN : public BRNElement { public:

  BRNCheckVLAN();
  ~BRNCheckVLAN();
  
  const char *class_name() const		{ return "BRNCheckVLAN"; }
  const char *port_count() const		{ return "1/1-2"; }
  const char *processing() const		{ return "a/ah"; }
  
  int configure(Vector<String> &, ErrorHandler *);


  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);

 private:
 
  AssocList *_assoc_list; /// _assoc_list is used to find the dst's associated SSID
  BRNVLAN *_brn_vlans; /// _brn_vlans is used to get the VLAN ID for the given SSID
  EtherAddress _me; /// my ether address

};

CLICK_ENDDECLS
#endif
