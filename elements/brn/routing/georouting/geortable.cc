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
 * GeorTable.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "geortable.hh"

CLICK_DECLS

GeorTable::GeorTable()
:_gps_map(NULL),_lt(NULL),_gps(NULL){
  BRNElement::init();
}

GeorTable::~GeorTable()
{
}

int
GeorTable::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "GPS", cpkP+cpkM, cpElement, &_gps,
      "GPSMAP", cpkP+cpkM, cpElement, &_gps_map,
      "LINKTABLE", cpkP+cpkM , cpElement, &_lt,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
GeorTable::initialize(ErrorHandler *)
{
  return 0;
}

GPSPosition *
GeorTable::getPosition(EtherAddress *ea)
{
  return (_gps_map->lookup(*ea));
}

/** TODO: find better solutzon for first itertion */
GPSPosition *
GeorTable::getClosest(struct gps_position *pos, EtherAddress *ea)
{
  EtherAddress acea,best;
  bool first = true;
  int min_dist = 2147483647l;
  GPSPosition *gpspos;
  GPSPosition finpos = GPSPosition(pos);

  for (GPSMap::EtherGPSMapIter i = _gps_map->_map.begin(); i.live();++i) {
    if ( first ) {
      best = i.key();
      gpspos = _gps_map->lookup(best);
      min_dist = finpos.getDistance(gpspos);
      first = false;
    } else {
      acea = i.key();
      gpspos = _gps_map->lookup(acea);
      if ( finpos.getDistance(gpspos) < min_dist ) {
        best = acea;
        min_dist = finpos.getDistance(gpspos);
      }
    }
  }

  if ( first ) return NULL;

  memcpy(ea->data(),acea.data(), 6);
  return (_gps_map->lookup(acea));
}

GPSPosition *
GeorTable::getClosestNeighbour(struct gps_position *pos, EtherAddress *ea)
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist

  GPSPosition finpos = GPSPosition(pos);

  int best;

  BRN_INFO("Search next hop for neighbor %s",finpos.unparse_coord().c_str());

  _lt->get_local_neighbors(neighbors);

  if ( neighbors.size() > 0 ) {
    //find the first neighbour
    int i = 0;
    int min_dist = -1;
    for (;i < neighbors.size(); i++) {
      if ( _lt->get_host_metric_from_me(neighbors[i]) < 700 ) {
        GPSPosition *gpspos = _gps_map->lookup(neighbors[i]);
        if ( gpspos == NULL ) continue;
        best = i;
        min_dist = finpos.getDistance(gpspos);
        BRN_INFO("Check distance to %s: %d",neighbors[0].unparse().c_str(),min_dist);
        break;
      } else {
        BRN_DEBUG("Bad metric: %s %d",neighbors[i].unparse().c_str(),_lt->get_host_metric_from_me(neighbors[i]));
      }
    }

    for( ; i < neighbors.size(); i++ ) {
      if ( _lt->get_host_metric_from_me(neighbors[i]) < 700 ) {
        GPSPosition *gpspos = _gps_map->lookup(neighbors[i]);
        if ( gpspos == NULL ) continue;
        BRN_INFO("Check distance to %s: %d",neighbors[i].unparse().c_str(),finpos.getDistance(gpspos));
        if ( finpos.getDistance(gpspos) < min_dist ) {
          best = i;
          min_dist = finpos.getDistance(gpspos);
        }
      } else {
        BRN_DEBUG("Bad metric: %s %d",neighbors[i].unparse().c_str(),_lt->get_host_metric_from_me(neighbors[i]));
      }
    }
    if ( min_dist == -1 ) {
      BRN_INFO("No good Neighbor. No neighbors: %d",neighbors.size());
      return NULL;
    }
  } else {
    BRN_INFO("No Neighbor");
    return NULL;
  }

  BRN_INFO("Best neighbour: %s: %d",neighbors[best].unparse().c_str(),finpos.getDistance(_gps_map->lookup(neighbors[best])));

  memcpy(ea->data(),neighbors[best].data(), 6);
  return (_gps_map->lookup(neighbors[best]));
}

GPSPosition *
GeorTable::getClosestNeighbour(GPSPosition *pos, EtherAddress *ea)
{
  struct gps_position stpos;
  pos->getPosition(&stpos);

  return getClosestNeighbour(&stpos,ea);
}
GPSPosition *
GeorTable::getLocalPosition()
{
  return _gps->getPosition();
}

void
GeorTable::updateEntry(EtherAddress *ea, struct gps_position *pos)
{
  GPSPosition *gpspos = _gps_map->lookup(*ea);

  if ( !gpspos)
    _gps_map->insert(*ea, GPSPosition(pos));
  else
    gpspos->setPosition(pos);
}

String
GeorTable::get_routing_table()
{
  StringAccum sa;

  sa << "Local:\t" << getLocalPosition()->_x << " " << getLocalPosition()->_y << " " << getLocalPosition()->_z << "\n";

  for (GPSMap::EtherGPSMapIter i = _gps_map->_map.begin(); i.live();++i) {
    EtherAddress ea = i.key();
    GPSPosition *gpspos = _gps_map->lookup(ea);

    sa << ea.unparse() << "\t" << gpspos->_x << " " << gpspos->_y << " " << gpspos->_z << "\n";
  }

  return sa.take_string();
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------
static int
write_addentry_param(const String &in_s, Element *e, void */*vparam*/,
                        ErrorHandler */*errh*/)
{
  GeorTable *gt = reinterpret_cast<GeorTable *>(e);
  EtherAddress ea;
  struct gps_position pos;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  cp_ethernet_address(args[0], &ea);
  cp_integer(args[1], &pos.x);
  cp_integer(args[2], &pos.y);
  cp_integer(args[3], &pos.z);

  gt->updateEntry(&ea, &pos);

  return 0;
}


static String
read_table_param(Element *e, void *)
{
  GeorTable *gt = reinterpret_cast<GeorTable *>(e);
  return gt->get_routing_table();
}

static String
read_debug_param(Element *e, void *)
{
  GeorTable *fl = reinterpret_cast<GeorTable *>(e);
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  GeorTable *fl = reinterpret_cast<GeorTable *>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
GeorTable::add_handlers()
{
  add_read_handler("table", read_table_param, 0);
  add_write_handler("add", write_addentry_param, 0);
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GeorTable)
