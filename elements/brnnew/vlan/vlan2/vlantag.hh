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

#ifndef CLICK_VLANTAG_HH
#define CLICK_VLANTAG_HH
#include <click/element.hh>

CLICK_DECLS
/**
 * This element tags an 802.1Q frame.
 * 
 * It sets the VLAN ID ranging from 0 to 4094 (inclusive) and the USER PRIORITY from 0
 * to 7 (inclusive).
 * 
 * Two VLAN IDs have a special meaning, which should be kept in mind.
 * - 0 means that the frame does not belong to any VLAN group. 
 *   This value is for using the user priority field only.
 * - 1 means that frame belongs to the default VLAN group
 * 
 * The values of USER PRIORITY field are specified in 802.??.
 * 
 * 
 * <h3>Usage</h3>
 * 
 * <pre>
 * ...
 * -> Print("802.1Q frame")
 * -> VLANTag(82) // tags a 802.1Q frame with VLAN ID 82 
 * -> Print("802.1Q frame")
 * ...
 * </pre>
 * 
 * Other usages:
 * <pre>
 * VLANTag(82, USER_PRIORITY 3) // tags a 802.1Q frame with VLAN ID 82 and a USER PRIORITY of 3
 * </pre>
 * 
 * The following syntax is equivalent to the above:
 * <pre>
 * VLANTag(VLANID 82, USER_PRIORITY 3)
 * </pre>
 * 
 * <h3>Handlers</h3>
 * 
 * - get_vlanid (read)
 * - get_user_priority (read)
 * - set_vlanid (write)
 * - set_user_priority (write)
 * 
 * <h3>In- and Output Ports</h3>
 * 
 * <pre>
 * input[0]        802.1Q
 * [0]output       802.1Q
 * </pre> 
 * @note
 * Incoming frames not matching the above specification are killed with an error message.
 * 
 * 
 * @todo
 * - Support setting of CFI bit, but this assumes that this is no ethernet frame, right? This
 *   bit is only set for Token Ring.
 *
 * <hr> 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-01 11:03:12 +0000 (Sun, 01 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2877 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/vlantag.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
class VLANTag : public Element { public:
  
  VLANTag();
  ~VLANTag();

  const char *class_name() const	{ return "VLANTag"; }
  const char *port_count() const	{ return PORTS_1_1; }
  const char *processing() const	{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }
  
  // Handlers
  void add_handlers();
  static String read_handler(Element *e, void *);  
  static int write_handler(const String &arg, Element *e, void *, ErrorHandler *errh);

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);
  
 private:
  
  uint16_t _vid; /// the VLAN ID set on the frame 
  uint8_t _prio; /// the USER PRIORITY set on the frame
  //bool _cfi;
  
};

CLICK_ENDDECLS
#endif
