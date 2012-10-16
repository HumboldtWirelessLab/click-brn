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

#ifndef GPSDECAPELEMENT_HH
#define GPSDECAPELEMENT_HH
#include <click/element.hh>
#include "elements/brn/brnelement.hh"

#include "gps_info.hh"

CLICK_DECLS

/*
=c
GPSDecap()

Strips outermost BRN header

=d

Removes the outermost BRN header from this packet.

=a CheckBRNHeader

*/

class GPSDecap : public BRNElement {

 public:

  GPSDecap();
  ~GPSDecap();

  const char *class_name() const  { return "GPSDecap"; }
  const char *port_count() const  { return PORTS_1_1; }

  Packet *smaction(Packet *);

  void push(int, Packet *p);
  Packet *pull(int);

};

CLICK_ENDDECLS
#endif
