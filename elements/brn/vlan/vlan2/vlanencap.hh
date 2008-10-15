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

#ifndef CLICK_VLANENCAP_HH
#define CLICK_VLANENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS
/**
 * This element converts an 802.3 frame into an 802.1Q frame.
 * 
 * It sets VLAN ID, USER PRIORITY and CFI to 0. The ethertype is copied
 * from the 802.3 frame to the field for encapsulated protocol. The protocol
 * field is set to 0x8100 (ETHERTYPE_8021Q).
 * 
 * @note
 * The TCI (VLAN ID, USER PRIORITY and CFI) are zeroed. You have to use VLANTag to set
 * a proper VLAN tag. 
 * 
 * <h3>Usage</h3>
 * 
 * <pre>
 * ...
 * -> Print("802.3 frame")
 * -> VLANEncap() 
 * -> Print("802.1Q frame")
 * ...
 * </pre>
 * 
 * <h3>In- and Output Ports</h3>
 * 
 * <pre>
 * input[0]        802.3
 * [0]output       802.1Q
 * </pre> 
 * @note
 * Incoming frames not matching the above specification are killed with an error message.
 * 
 *
 * <hr> 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-01 11:03:12 +0000 (Sun, 01 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2877 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/vlanencap.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
class VLANEncap : public Element { public:
  
  VLANEncap();
  ~VLANEncap();

  const char *class_name() const	{ return "VLANEncap"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
 private:
  
};

CLICK_ENDDECLS
#endif
