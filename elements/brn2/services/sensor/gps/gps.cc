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

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "gps.hh"

CLICK_DECLS

GPS::GPS()
  :_gpsmode(GPSMODE_HANDLER)
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
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
GPS::initialize(ErrorHandler *)
{
  return 0;
}

void
GPS::push( int /*port*/, Packet *packet )
{
  packet->kill();
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
  sa << "<gps id='" << BRN_NODE_NAME << "' time='" << Timestamp::now().unparse();
  sa << "' lat='" << _position._latitude.unparse() << "' long='" << _position._longitude.unparse();
  sa << "' alt='" << _position._altitude.unparse() << "' speed='" << _position._speed.unparse() << "' />\n";
  return sa.take_string();
}

static String
read_position_param(Element *e, void *thunk)
{
  StringAccum sa;
  GPS *gps = (GPS *)e;

  GPSPosition *pos = gps->getPosition();
  switch ((uintptr_t) thunk)
  {
    case H_CART_COORD: {
      sa << pos->_x << " " << pos->_y << " " << pos->_z;
      break;
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
  int x,y,z;
  GPS *gps = (GPS *)e;
  GPSPosition *pos = gps->getPosition();
  switch ((uintptr_t) thunk)
  {
    case H_CART_COORD: {
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      cp_integer(args[0], &x);
      cp_integer(args[1], &y);

      if ( args.size() > 2 )
        cp_integer(args[2], &z);
      else
        z = 0;

      pos->setCC(x,y,z);
      break;
    }
    case H_GPS_COORD: {
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      if ( args.size() > 2 ) {
        pos->setGPSC(args[0], args[1], args[2]);
      } else {
        pos->setGPSC(args[0],args[1], "0.0");
      }

      break;
    }
    case H_SPEED: {
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      pos->setSpeed(args[0]);
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
