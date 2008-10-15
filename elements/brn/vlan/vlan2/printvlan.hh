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

#ifndef CLICK_PRINTVLAN_HH
#define CLICK_PRINTVLAN_HH
#include <click/element.hh>
CLICK_DECLS

/**
 * This element prints the VLAN ID, USER PRIORITY and CFI field of a 802.1Q frame.
 * 
 * <h3>Usage</h3>
 * 
 * <pre>
 * ...
 * -> Print("802.1Q frame")
 * -> PrintVLAN()
 * -> Print("802.1Q frame")
 * ...
 * </pre>
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
 * @todo
 * - Print also the protocol type of the encaped frame
 * - What about configurations? Possibility to enter a label or decide what values (VID, PRIO,..)
 *   to output
 *  
 * <hr>
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-01 11:03:12 +0000 (Sun, 01 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2877 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/printvlan.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
 
class PrintVLAN : public Element { public:

  PrintVLAN();
  ~PrintVLAN();
  
  const char *class_name() const		{ return "PrintVLAN"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  Packet *simple_action(Packet *);
  
 private:

};

CLICK_ENDDECLS
#endif
