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
 * gps.{cc,hh}
 */

#include <click/config.h>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#if CLICK_NS
#include <click/router.hh>
#endif

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "gps.hh"

CLICK_DECLS

GPS::GPS()
  :_gpsmode(GPSMODE_HANDLER),
   _position(),
   _gpsmap(NULL),
#if CLICK_NS
   gps_timer(this),
#endif
   _interval(DEFAULT_GPS_UPDATE_INTERVAL)
{
  BRNElement::init();
}

GPS::~GPS()
{
}

int
GPS::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "GPSMODE", cpkP, cpInteger, &_gpsmode,
      "GPSMAP", cpkP, cpElement, &_gpsmap,
      "UPDATEINTEVAL", cpkP, cpElement, &_interval,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
GPS::initialize(ErrorHandler *)
{
#if CLICK_NS
  gps_timer.initialize(this);

  if ( _interval > 0 ) {
    gps_timer.schedule_after_msec(_interval);
  }
#endif

  return 0;
}

void
GPS::run_timer(Timer*)
{
#if CLICK_NS
  int pos[4];
  simclick_sim_command(router()->simnode(), SIMCLICK_GET_NODE_POSITION, &pos);
  _position.setCC(pos[0],pos[1],pos[2]);
  _position.setSpeed(pos[3]);
  gps_timer.schedule_after_msec(_interval);
#endif
}

/* Used for communication with gpsd,... */
void
GPS::push( int /*port*/, Packet *packet )
{
  packet->kill();
}

void
GPS::updateMap()
{
  if ( _gpsmap ) {
    _gpsmap->insert(EtherAddress(), _position);
  }
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_CART_COORD,
  H_GPS_COORD,
  H_SPEED
};

String
GPS::read_gps()
{
  StringAccum sa;
  sa << "<gps id=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now().unparse();
  sa << "\" lat=\"" << _position._latitude.unparse() << "\" long=\"" << _position._longitude.unparse();
  sa << "\" alt=\"" << _position._altitude.unparse() << "\" speed=\"" << _position._speed.unparse() << "\" />\n";
  return sa.take_string();
}


String
GPS::read_cart()
{
  StringAccum sa;
  sa << "<gps id=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now().unparse();
  sa << "\" x=\"" << _position._x << "\" y=\"" << _position._y;
  sa << "\" z=\"" << _position._z << "\" speed=\"" << _position._speed.unparse() << "\" />\n";
  return sa.take_string();
}

static String
read_position_param(Element *e, void *thunk)
{
  StringAccum sa;
  GPS *gps = reinterpret_cast<GPS *>(e);

  switch ((uintptr_t) thunk)
  {
    case H_CART_COORD: {
      return gps->read_cart();
    }
    case H_GPS_COORD: {
      return gps->read_gps();
    }
  }
  return sa.take_string();
}

static int
write_position_param(const String &in_s, Element *e, void *thunk, ErrorHandler */*errh*/)
{
  GPS *gps = reinterpret_cast<GPS *>(e);
  GPSPosition *pos = gps->getPosition();
  switch ((uintptr_t) thunk)
  {
    case H_CART_COORD: {
      int x,y,z;
      x = y = z = 0;
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      cp_integer(args[0], &x);
      cp_integer(args[1], &y);

      if ( args.size() > 2 ) cp_integer(args[2], &z);

      pos->setCC(x,y,z);

      gps->updateMap();

      break;
    }
    case H_GPS_COORD: {
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      if ( args.size() > 2 ) {
        pos->setGPS(args[0], args[1], args[2]);
      } else {
        pos->setGPS(args[0],args[1], "0.0");
      }

      gps->updateMap();

      break;
    }
    case H_SPEED: {
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      pos->setSpeed(args[0]);

      gps->updateMap();

      break;
    }
  }

  return 0;
}

void
GPS::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("cart_coord", read_position_param, H_CART_COORD);
  add_write_handler("cart_coord", write_position_param, H_CART_COORD);

  add_read_handler("gps_coord", read_position_param, H_GPS_COORD);
  add_write_handler("gps_coord", write_position_param, H_GPS_COORD);

  add_write_handler("speed", write_position_param, H_SPEED);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPS)
