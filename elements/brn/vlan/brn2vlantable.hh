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

#ifndef CLICK_BRN2VLANTABLE_HH
#define CLICK_BRN2VLANTABLE_HH
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS

/**
 * Stores the src and vid.
 * 
 * @see StoreVLAN
 * 
 * <hr> 
 * @author J. Mueller <jmueller@informatik.hu-berlin.de>
 * @date $LastChangedDate: 2006-10-12 08:47:38 +0000 (Thu, 12 Oct 2006) $ by $Author: jmueller $ in revision $Rev: 2921 $ at $HeadURL: svn://brn-svn/BerlinRoofNet/branches/click-vlan_gateway/click-core/elements/vlan/vlantable.hh $
 * @version 0.5
 * @since 0.5
 * 
 */
class BRN2VLANTable : public Element { public:

  BRN2VLANTable();
  ~BRN2VLANTable();

  const char *class_name() const		{ return "BRN2VLANTable"; }
  const char *port_count() const		{ return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);

  void add_handlers();

  String all_vlans();
  void insert(EtherAddress ea, uint16_t id) { _vlans.insert(ea,id); }
  uint16_t find(EtherAddress ea) { return _vlans.find(ea, 0xFFFF); }

  int _debug;

  typedef HashMap<EtherAddress, uint16_t> VlanTable;
  typedef VlanTable::const_iterator VTIter;

 private:
  friend class StoreVLAN;
  friend class RestoreVLAN;

  VlanTable _vlans; /// stores src -> vid
};

CLICK_ENDDECLS
#endif
