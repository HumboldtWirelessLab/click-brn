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

#ifndef CLICK_CHECKVLAN_HH
#define CLICK_CHECKVLAN_HH
#include <click/element.hh>
#include <click/bighashmap.hh>
CLICK_DECLS

/**
 * Outputs a 802.1Q frame according to its VLAN ID.
 * 
 * In the configuration string VLAN IDs are specified. If e.g. the second VLAN ID matches
 * the frame will be outputed to the second output port (named output port 1). The last arguement
 * can be a '-' to denote match all VLAN ID. Thus frames not matched by any specified VLAN ID are outputed
 * to the last output port. If this option isn't used, the frames not matching will be killed.
 * 
 * @note
 * It is not possible to specify a VLAN ID more than once.
 * 
 * <h3>Usage</h3>
 * 
 * <pre>
 * ...
 * -> Print("802.1Q frame")
 * -> ckeck_vlan :: CheckVLAN(0, 1, 42, 72, 6, 123, -);
 * 
 * check_vlan[0]
 * -> Print("802.Q frame with VLAN ID 0")
 * ...
 * 
 * check_vlan[1]
 * -> Print("802.Q frame with VLAN ID 0")
 * ...
 * 
 * ...
 * 
 * check_vlan[6]
 * -> Print("802.Q frame, which VLAN ID isn't 0, 1, 6, 42, 72 or 123")
 * ...
 * </pre>
 * 
 * <h3>In- and Output Ports</h3>
 * 
 * <pre>
 * input[0]        802.1Q
 * [0]output       802.1Q
 *   ...
 *   ...
 *   ...
 * [n]output       802.1Q
 * </pre>
 * 
 * @note
 * Incoming frames not matching the above specification are killed with an error message.
 * 
 * <hr> 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-12 08:47:38 +0000 (Thu, 12 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2921 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/checkvlan.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
class CheckVLAN : public Element { public:

  CheckVLAN();
  ~CheckVLAN();
  
  const char *class_name() const		{ return "CheckVLAN"; }
  const char *port_count() const		{ return "1/-"; }
  const char *processing() const		{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  void push(int port, Packet *);
  
 private:
  HashMap<uint16_t, int> _vlan_ids; /// stores the configured VLAN IDs and the appropriate port
};

CLICK_ENDDECLS
#endif
