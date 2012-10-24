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

#ifndef BOIDELEMENT_HH
#define BOIDELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/ipaddress.hh>
#include <click/timer.hh>

#include <elements/brn/brnelement.hh>
#include <elements/brn/standard/fixpointnumber.hh>

#include "elements/brn/services/sensor/gps/gps.hh"

#include <elements/brn/services/sensor/gps/gps_position.hh>
#include <elements/brn/services/sensor/gps/gps_map.hh>

#include "mobility.hh"
#include "elements/brn/wifi/channelstats.hh"


CLICK_DECLS
/*
 * =c
 * Boid()
 * =s
 * =d
 *
 */

#define BOID_DEFAULT_INTERVAL 1000
#define BOID_DEFAULT_RADIUS 80
#define BOID_DEFAULT_STEERLIMIT 0.1
#define BOID_DEFAULT_GRAVITATION 10.0
#define BOID_DEFAULT_COHESION 10.0
#define BOID_DEFAULT_SEPERATION 10.0
#define BOID_DEFAULT_SPEED 10

class Boid : public BRNElement {

 public:

  class Gravitation {
    public:
      Vector3D _position;
      int _mass;

      Gravitation() {
        _position = Vector3D();
        _mass = 0;
      }

      Gravitation(int x, int y, int z, int m) {
        _position = Vector3D(x,y,z);
        _mass = m;
      }

      ~Gravitation() {};
  };

  typedef Vector<Gravitation> GravitationList;
  typedef GravitationList::const_iterator GravitationListIter;

  class Predator {
    public:
      Vector3D _position;
      int _power;

      Predator() {
        _position = Vector3D();
        _power = 0;
      }

      Predator(int x, int y, int z, int p) {
        _position = Vector3D(x,y,z);
        _power = p;
      }

      ~Predator() {};
  };

  typedef Vector<Predator> PredatorList;
  typedef PredatorList::const_iterator PredatorListIter;

  //
  //methods
  //
  Boid();
  ~Boid();

  const char *class_name() const  { return "Boid"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *);
  void compute_behavior();

  int find_gravitation(int x, int y, int z, int m);
  void add_gravitation(int x, int y, int z, int m);
  void del_gravitation(int x, int y, int z, int m);

  int find_predator(int x, int y, int z, int m);
  void add_predator(int x, int y, int z, int m);
  void del_predator(int x, int y, int z, int m);

  Timer _boid_timer;

  Mobility *_mob;
  GPS *_gps;
  GPSMap *_gpsmap;
  ChannelStats *_cs;

  int _interval;

  bool _active;

  void set_active(bool active) { _active = active; };

  int _radius;
  double _cohesion;
  double _gravitation;
  double _steerlimit;
  double _seperation;
  int _speed;

  GravitationList _glist;
  PredatorList _plist;
};

CLICK_ENDDECLS
#endif
