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

/*
 * boid.{cc,hh}
 */

#include <click/config.h>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#if CLICK_NS
#include <click/router.hh>
#endif

#include "elements/brn/standard/vector/vector3d.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "boid.hh"
#include "boidbehavior/boidbehavior.hh"

CLICK_DECLS

Boid::Boid():
  _boid_timer(this),
  _interval(BOID_DEFAULT_INTERVAL),
  _active(false)
{
  BRNElement::init();
}

Boid::~Boid()
{
  _glist.clear();
  _plist.clear();
}

int
Boid::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "BEHAVIOR", cpkM+cpkP, cpElement, &_behavior,
      "GPS", cpkM+cpkP, cpElement, &_gps,
      "GPSMAP", cpkM+cpkP, cpElement, &_gpsmap,
      "MOBILITY", cpkM+cpkP, cpElement, &_mob,
      "INTERVAL", cpkP, cpInteger, &_interval,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
Boid::initialize(ErrorHandler *)
{
  _glist.clear();
  _plist.clear();

  _boid_timer.initialize(this);
  _boid_timer.schedule_after_msec(_interval);

  return 0;
}

void
Boid::run_timer(Timer *) {

  _boid_timer.reschedule_after_msec(_interval);

  if ( _active ) {
    GPSPosition *own_pos = _gps->getPosition();
    BoidMove *bm = _behavior->compute_behavior(own_pos, _gpsmap, _glist, _plist);
    _mob->move(bm->_direction._x, bm->_direction._y, bm->_direction._z, bm->_speed, bm->_move_type);
  }
}

int
Boid::find_gravitation(int x, int y, int z, int m)
{
  for (int i = 0; i < _glist.size(); i++) {
    Gravitation g = _glist[i];
    if ( g._position.equals(Vector3D(x,y,z)) && ( g._mass == m ) ) return i;
  }

  return -1;
}

void
Boid::add_gravitation(int x, int y, int z, int m)
{
  int index = find_gravitation(x,y,z,m);
  if ( index != -1 ) {
    BRN_DEBUG("Gravitation already in list");
    return;
  }

  _glist.push_back(Gravitation(x, y, z, m));

}

void
Boid::del_gravitation(int x, int y, int z, int m)
{
  int index = find_gravitation(x,y,z,m);
  if ( index == -1 ) {
    BRN_DEBUG("Gravitation not found");
    return;
  }

  _glist.erase(_glist.begin() + index);
}

int
Boid::find_predator(int x, int y, int z, int p)
{
  for (int i = 0; i < _plist.size(); i++) {
    Predator pred = _plist[i];
    if ( pred._position.equals(Vector3D(x,y,z)) && ( pred._power == p ) ) return i;
  }

  return -1;
}

void
Boid::add_predator(int x, int y, int z, int p)
{
  int index = find_predator(x,y,z,p);
  if ( index != -1 ) {
    BRN_DEBUG("Predator already in list");
    return;
  }

  _plist.push_back(Predator(x, y, z, p));

}

void
Boid::del_predator(int x, int y, int z, int p)
{
  int index = find_predator(x,y,z,p);
  if ( index == -1 ) {
    BRN_DEBUG("Predator not found");
    return;
  }

  _plist.erase(_plist.begin() + index);
}
//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_GRAVITATION,
  H_PREDATOR,
  H_STATS,
  H_ACTIVE
};


static String
Boid_read_param(Element *e, void *thunk)
{
  Boid *b = (Boid *)e;

  switch ((uintptr_t) thunk) {
    case H_GRAVITATION: {
      StringAccum sa;
      sa << "<boid_gravitation count=\"" << b->_glist.size() << "\" time=\"" << Timestamp::now().unparse() << "\" >\n";
      for (int i = 0; i < b->_glist.size(); i++) {
        Gravitation g = b->_glist[i];
        sa << "\t<gravitation index=\"" << i << "\" x=\"" << g._position._x << "\" y=\"" << g._position._y;
        sa << "\" z=\"" << g._position._z << "\" mass=\"" << g._mass << "\" />\n";
      }
      sa << "</boid_gravitation>\n";

      return sa.take_string();
    }
    case H_PREDATOR: {
      StringAccum sa;
      sa << "<boid_predator count=\"" << b->_plist.size() << "\" time=\"" << Timestamp::now().unparse() << "\" >\n";
      for (int i = 0; i < b->_plist.size(); i++) {
        Predator p = b->_plist[i];
        sa << "\t<predator index=\"" << i << "\" x=\"" << p._position._x << "\" y=\"" << p._position._y;
        sa << "\" z=\"" << p._position._z << "\" power=\"" << p._power << "\" />\n";
      }
      sa << "</boid_predator>\n";

      return sa.take_string();
    }
    case H_STATS: {
      //return b->stats();
    }
    case H_ACTIVE: {
      return String(b->_active);
    }
    default:
      return String();
  }
}

static int
Boid_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Boid *boid = (Boid *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {

    case H_ACTIVE: {
      Vector<String> args;
      cp_spacevec(s, args);

      bool active;
      cp_bool(args[0], &active);

      boid->set_active(active);
      break;
    }
    case H_GRAVITATION: {
      Vector<String> args;
      cp_spacevec(s, args);

      if ( args.size() < 5 ) return errh->error("Gravitaion requires 5 params");

      int x, y, z, m;

      cp_integer(args[1], &x);
      cp_integer(args[2], &y);
      cp_integer(args[3], &z);
      cp_integer(args[4], &m); //mass

      if ( args[0] == "add" ) boid->add_gravitation(x,y,z,m);
      else boid->del_gravitation(x,y,z,m);

      break;
    }
    case H_PREDATOR: {
      Vector<String> args;
      cp_spacevec(s, args);

      if ( args.size() < 5 ) return errh->error("Predator requires 5 params");

      int x, y, z, p;

      cp_integer(args[1], &x);
      cp_integer(args[2], &y);
      cp_integer(args[3], &z);
      cp_integer(args[4], &p); //mass

      if ( args[0] == "add" ) boid->add_predator(x,y,z,p);
      else boid->del_predator(x,y,z,p);

      break;
    }
  }
  return 0;
}


void
Boid::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("active", Boid_read_param, (void *)H_ACTIVE);
  add_read_handler("stats", Boid_read_param, (void *)H_STATS);
  add_read_handler("gravitation", Boid_read_param, (void *)H_GRAVITATION);
  add_read_handler("predator", Boid_read_param, (void *)H_PREDATOR);

  add_write_handler("active", Boid_write_param, (void *)H_ACTIVE);
  add_write_handler("gravitation", Boid_write_param, (void *)H_GRAVITATION);
  add_write_handler("predator", Boid_write_param, (void *)H_PREDATOR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Vector3D)
EXPORT_ELEMENT(Boid)
