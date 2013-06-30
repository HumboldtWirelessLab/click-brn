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

#ifndef BOIDBEHAVIORELEMENT_HH
#define BOIDBEHAVIORELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>

#include <elements/brn/brnelement.hh>

#include "elements/brn/services/sensor/gps/gps.hh"
#include <elements/brn/services/sensor/gps/gps_position.hh>
#include <elements/brn/services/sensor/gps/gps_map.hh>

#include <elements/brn/standard/fixpointnumber.hh>

#include <elements/brn/services/mobility/boid_helper.hh>

CLICK_DECLS
/*
 * =c
 * BoidBehavior()
 * =s
 * =d
 *
 */

class BoidBehavior : public BRNElement {

 public:

  //
  //methods
  //
  BoidBehavior();
  ~BoidBehavior();

  void add_handlers();

  virtual const char *behavior_name() const = 0; //const : function doesn't change the object (members).
                                                //virtual: late binding
  /**
   * boid_helper.hh
   *
   * class BoidMove {
   *  public:
   *   Vector3D _direction;
   *   int      _speed;
   *   int      _move_type;
   * };
  **/
  virtual BoidMove* compute_behavior(GPSPosition *own_pos, GPSMap *gpsmap, GravitationList &glist, PredatorList &plist) = 0;

};

CLICK_ENDDECLS
#endif
