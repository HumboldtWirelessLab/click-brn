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

#ifndef CLICK_STOREVLAN_HH
#define CLICK_STOREVLAN_HH
#include <click/element.hh>
#include <click/bighashmap.hh>
CLICK_DECLS

class VLANTable;

/**
 * Stores the src and its vid of a 802.1Q frame in a VLANTable.
 * 
 * If this is no 802.1Q frame it will be outputed to the second output port,
 * if available, else the packet is killed.
 * 
 * <h3>Usage</h3>
 * 
 * <pre>
 * table :: VLANTable();
 * ...
 * -> Print("802.1Q frame")
 * -> store_vlan :: StoreVLAN(table);
 * ...
 * 
 * 
 * ...
 * -> Print("802.3 frame")
 * -> RestoreVLAN(table)
 * ...
 * </pre>
 * 
 * <h3>In- and Output Ports</h3>
 * 
 * <pre>
 * input[0]        802.1Q
 * [0]output       802.1Q
 * [1]output       incoming frame
 * </pre>
 * 
 * @note
 * Incoming frames not matching the above specification are killed or
 * outputed to the second output port.
 * 
 * <hr> 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-12 08:47:38 +0000 (Thu, 12 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2921 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/storevlan.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
class StoreVLAN : public Element { public:

  StoreVLAN();
  ~StoreVLAN();
  
  const char *class_name() const		{ return "StoreVLAN"; }
  const char *port_count() const		{ return "1/1-2"; }
  const char *processing() const		{ return PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);

  void push(int port, Packet *);
  
 private:
  VLANTable *_table;
};

CLICK_ENDDECLS
#endif
