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

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef TOPOLOGY_DIBADAWN_SEARCHID_HH
#define TOPOLOGY_DIBADAWN_SEARCHID_HH

#include <click/element.hh>

#include "topology_info.hh"

CLICK_DECLS;

class DibadawnSearchId {
public:
  DibadawnSearchId(Timestamp t, const EtherAddress *creator);
  DibadawnSearchId();
  String AsString();
  uint8_t* PointerTo10BytesOfData();

private:
  uint8_t data[10];
};

CLICK_ENDDECLS
#endif
