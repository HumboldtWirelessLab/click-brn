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

#ifndef MOBILITYELEMENT_HH
#define MOBILITYELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>

#include <elements/brn2/brnelement.hh>
#include <elements/brn2/standard/fixpointnumber.hh>

#include <elements/brn2/services/sensor/gps/gps_position.hh>
#include <elements/brn2/services/sensor/gps/gps_map.hh>


CLICK_DECLS
/*
 * =c
 * Mobility()
 * =s
 * =d
 *
 */

#define MOVE_TYPE_ABSOLUTE 0
#define MOVE_TYPE_RELATIVE 1

class Mobility : public BRNElement {

 public:

  //
  //methods
  //
  Mobility();
  ~Mobility();

  const char *class_name() const  { return "Mobility"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void move(int x, int y, int z, int speed, int move_type = MOVE_TYPE_ABSOLUTE);
  //void move(GPSPosition *gpspos, int move_type = MOVE_TYPE_ABSOLUTE);

};

CLICK_ENDDECLS
#endif
