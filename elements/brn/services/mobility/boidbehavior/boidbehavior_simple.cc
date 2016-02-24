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

#include <elements/brn/standard/vector/vector3d.hh>
#include <elements/brn/standard/brnlogger/brnlogger.hh>

#include <elements/brn/services/mobility/mobility.hh>

#include "boidbehavior_simple.hh"
#include "../../sensor/gps/gps_map.hh"


CLICK_DECLS

BoidBehaviorSimple::BoidBehaviorSimple():
  _cs(NULL),
  boidmove(BoidMove()),
  _interval(0),
  _radius(BOID_DEFAULT_RADIUS),
  _cohesion(BOID_DEFAULT_COHESION),
  _gravitation(BOID_DEFAULT_GRAVITATION),
  _steerlimit(BOID_DEFAULT_STEERLIMIT),
  _seperation(BOID_DEFAULT_SEPERATION),
  _speed(BOID_DEFAULT_SPEED)
{
  BRNElement::init();
}

BoidBehaviorSimple::~BoidBehaviorSimple()
{
}

int
BoidBehaviorSimple::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int steerlim = 0;
  int cohesion = 0;
  int gravitation = 0;
  int seperation = 0;

  if (cp_va_kparse(conf, this, errh,
      "CHANNELSTATS", cpkM+cpkP, cpElement, &_cs,
      "RADIUS", cpkP, cpInteger, &_radius,
      "SEPERATIONSFACTOR", cpkP, cpInteger, &seperation,
      "COHESIONFACTOR", cpkP, cpInteger, &cohesion,
      "STEERLIMIT", cpkP, cpInteger, &steerlim,
      "GRAVITATIONFACTOR", cpkP, cpInteger, &gravitation,
      "SPEED", cpkP, cpInteger, &_speed,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  if ( steerlim != 0 ) _steerlimit = steerlim / 1000.0;
  if ( cohesion != 0 ) _cohesion = cohesion / 1000.0;
  if ( gravitation != 0 ) _gravitation = gravitation / 1000.0;
  if ( seperation != 0 ) _seperation = seperation / 1000.0;

  return 0;
}

int
BoidBehaviorSimple::initialize(ErrorHandler *)
{
  return 0;
}

static Vector3D gps_dist(GPSPosition *pos1, GPSPosition *pos2) {
  return Vector3D(pos1->_x - pos2->_x, pos1->_y - pos2->_y, pos1->_z - pos2->_z);
}

BoidMove*
BoidBehaviorSimple::compute_behavior(GPSPosition *own_pos, GPSMap *gpsmap, GravitationList &glist, PredatorList &plist)
{
  Vector3D velo;
  Vector3D possum;

  Vector3D grav;
  Vector3D sep;
  Vector3D coh;
  Vector3D pred;

  grav.zero();
  sep.zero();
  coh.zero();
  pred.zero();

  int _minradius = _radius;
  
  int sepfac = 1;
  int cohfac = 1;
  int gravfac = 1;
  int predfac = 1;
  
  
  /**********************************************************/
  /*                     1. Separation                      */
  /**********************************************************/
  ChannelStats::SrcInfoTable *src_tab = _cs->get_latest_stats_neighbours();

  for (ChannelStats::SrcInfoTableIter iter = src_tab->begin(); iter.live(); ++iter) {
    //ChannelStats::SrcInfo src = iter.value();
    EtherAddress ea = iter.key();

    GPSPosition *pos = gpsmap->lookup(ea);

    if ( pos != NULL ) {
      Vector3D dist = gps_dist(own_pos, pos);
      double dist_len = dist.length();

      BRN_DEBUG("Dist: %s",dist.unparse().c_str());
		
	  if(dist_len < _minradius){
		cohfac = 0;
		gravfac = 0;
		predfac = 0;
	  }
		
      if ((dist_len > 0) && (dist_len < _radius)) {
        dist.normalize();                 //repulse.normalize();
        BRN_DEBUG("Dist_vec_norm: %s", dist.unparse().c_str());
        //dist.div(dist_len);            //repulse.div(dist);
        sep.add(dist);              //posSum.add(repulse);
      }
    } else {
      BRN_WARN("No GPS-Position for %s.",ea.unparse().c_str());
    }
  }

  sep.mul(_seperation);

  BRN_DEBUG("Sep: %s", velo.unparse().c_str());

  /**********************************************************/
  /*                     2. Cohesion                        */
  /**********************************************************/
 
  int count = 0;

  //for(SwarmObject neighbor :swarmCluster.get(clus)) {
  for (ChannelStats::SrcInfoTableIter iter = src_tab->begin(); iter.live(); ++iter) {

    //ChannelStats::SrcInfo src = iter.value();
    EtherAddress ea = iter.key();

    GPSPosition *pos = gpsmap->lookup(ea);

    if ( pos != NULL ) {
      Vector3D dist = gps_dist(own_pos,pos);     //float dist = swObj.getDistance(neighbor);

      if ((dist.length() > 0) && (dist.length() < _radius)) { //if ((dist > 0) && (dist < parent.getZoneRadius())) {
        Vector3D gps_vec = pos->vector3D();
        coh.add(gps_vec);             // posSum.add(neighbor.getPosition())
        count++;
      }
    } else {
      BRN_WARN("No GPS-Position for %s.",ea.unparse().c_str());
    }
  }

  if (count > 0) coh.div(count);

  Vector3D own_gps_vec = own_pos->vector3D();
  coh.sub(own_gps_vec);      // PVector steer = PVector.sub(posSum, swObj.getPosition());
  coh.limit(_steerlimit);    //steer.limit(0.1f);
  coh.mul(_cohesion);        //swObj.getAcceleration().add(PVector.mult(steer, parent.getCohesionFactor()));

  BRN_DEBUG("Coh: %s", velo.unparse().c_str());

  /**********************************************************/
  /*                     3. Alignment                       */
  /**********************************************************/

/*  posSum = new PVector(0,0,0);
  count = 0;
  for (SwarmObject neighbor : swarmCluster.get(clus)) {
    float dist = swObj.getDistance(neighbor);
    if ((dist > 0) && (dist < parent.getZoneRadius())) {
      posSum.add(neighbor.getVelocity());
      count++;
    }
  }
  if (count > 0) {
    posSum.div((float)count);
    posSum.limit(0.1f);
  }
  swObj.getAcceleration().add(PVector.mult(posSum, parent.getAlignmentFactor()));

*/
  /**********************************************************/
  /*        4. Pull to Center with Gravitation              */
  /**********************************************************/

  count = 0;

  for (int i = 0; i < glist.size(); i++) {
    Gravitation g = glist[i];
    Vector3D dist = own_pos->vector3D();
    dist.sub(g._position);
    dist.mul(-1);
    double dist_len = dist.length();
    BRN_DEBUG("Grav Dist: %s",dist.unparse().c_str());
    if (dist_len > 0) {
      dist.normalize();            //repulse.normalize();
      BRN_DEBUG("Grav Dist_vec_norm: %s", dist.unparse().c_str());
      dist.div(dist_len);        //repulse.div(dist);
      grav.add(dist);          //posSum.add(repulse);
      count++;
    }
  }

  if (count > 0) {
    grav.div(count);

    Vector3D own_gps_vec = own_pos->vector3D();
    grav.limit(_steerlimit);    //steer.limit(0.1f);
    grav.mul(_gravitation);

  }

  BRN_DEBUG("grav: %s", velo.unparse().c_str());

  /**********************************************************/
  /*              5. Move away from Predators               */
  /**********************************************************/
  count = 0;

  for (int i = 0; i < plist.size(); i++) {
    Predator p = plist[i];
    Vector3D dist = own_pos->vector3D();
    Vector3D p_pos = p._position;
    dist.sub(p_pos);
    double dist_len = dist.length();
    BRN_DEBUG("Dist: %s",dist.unparse().c_str());
    if (dist_len > 0) {
      dist.normalize();            //repulse.normalize();
      BRN_DEBUG("Dist_vec_norm: %s", dist.unparse().c_str());
      if ( dist_len != 0 ) {
        dist.div(dist_len);        //repulse.div(dist);
        pred.add(dist);          //posSum.add(repulse);
        count++;
      }
    }
  }

  if (count > 0) {
    pred.div(count);

    own_gps_vec = own_pos->vector3D();
    pred.limit(_steerlimit);    //steer.limit(0.1f);
  }

  BRN_DEBUG("pred: %s", velo.unparse().c_str());

  sep.mul(sepfac);
  coh.mul(cohfac);
  grav.mul(gravfac);
  pred.mul(predfac);
  
  velo.add(sep);
  velo.add(coh);
  velo.add(grav);
  velo.add(pred);
    
  velo.mul(_speed);
  
  boidmove._direction._x = round(velo._x);
  boidmove._direction._y = round(velo._y);
  boidmove._direction._z = 0;
  boidmove._speed = _speed;
  boidmove._move_type = MOVE_TYPE_RELATIVE;
  
  return &boidmove;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_STATS
};

static String
BoidBehaviorSimple_read_param(Element */*e*/, void *thunk)
{
  //BoidBehaviorSimple *b = reinterpret_cast<BoidBehaviorSimple *>(e);

  switch ((uintptr_t) thunk) {
    case H_STATS: {
      //return b->stats();
    }
    default:
      return String();
  }
}

void
BoidBehaviorSimple::add_handlers()
{
  BoidBehavior::add_handlers();

  add_read_handler("stats", BoidBehaviorSimple_read_param, (void *)H_STATS);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Vector3D)
EXPORT_ELEMENT(BoidBehaviorSimple)
