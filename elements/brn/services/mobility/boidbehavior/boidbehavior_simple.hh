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

#ifndef BOIDBEHAVIORSIMPLEELEMENT_HH
#define BOIDBEHAVIORSIMPLEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>

#include "boidbehavior.hh"
#include <elements/brn/standard/fixpointnumber.hh>

#include "elements/brn/services/sensor/gps/gps.hh"

#include <elements/brn/services/sensor/gps/gps_position.hh>
#include <elements/brn/services/sensor/gps/gps_map.hh>

#include "elements/brn/wifi/rxinfo/channelstats.hh"


CLICK_DECLS
/*
 * =c
 * BoidBehaviorSimple()
 * =s
 * =d
 *
 */

#define BOID_DEFAULT_RADIUS 80
#define BOID_DEFAULT_STEERLIMIT 0.1
#define BOID_DEFAULT_GRAVITATION 10.0
#define BOID_DEFAULT_COHESION 10.0
#define BOID_DEFAULT_SEPERATION 10.0
#define BOID_DEFAULT_SPEED 10

class BoidBehaviorSimple : public BoidBehavior {

 public:
  //
  //methods
  //
  BoidBehaviorSimple();
  ~BoidBehaviorSimple();

  const char *class_name() const { return "BoidBehaviorSimple"; }
  const char *behavior_name() const { return "BoidBehaviorSimple"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  BoidMove* compute_behavior(GPSPosition *own_pos, GPSMap *gpsmap, GravitationList &glist, PredatorList &plist);

  ChannelStats *_cs;

  BoidMove boidmove;

  int _interval;

  int _radius;
  double _cohesion;
  double _gravitation;
  double _steerlimit;
  double _seperation;
  int _speed;
};

CLICK_ENDDECLS
#endif
