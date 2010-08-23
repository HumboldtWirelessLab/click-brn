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
  H_GPS_COORD
};

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
      sa << pos->_latitude / 1000000 << "." << pos->_latitude % 1000000 << " ";
      sa << pos->_longitude / 1000000 << "." << pos->_longitude % 1000000 << " ";
      sa << pos->_h / 1000000 << "." << pos->_h % 1000000;
      break;
    }
  }
  return sa.take_string();
}

static int sizeup(int i) {
  if ( i <= 0 ) return i;
  while ( i < 100000 ) i *= 10;
  return i;
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

      int pointpos = args[0]. find_left('.',0);
      String prep = args[0].substring(0, pointpos);
      String postp = args[0].substring(pointpos+1,args[0].length());

      int prei, posti;
      cp_integer(prep, &prei);
      cp_integer(postp, &posti);

      x = prei * 1000000 + sizeup(posti);
      //click_chatter("LAT: %s.%s  %d",prep.c_str(),postp.c_str(), x);

      pointpos = args[1]. find_left('.',0);
      prep = args[1].substring(0, pointpos);
      postp = args[1].substring(pointpos+1,args[1].length());

      cp_integer(prep, &prei);
      cp_integer(postp, &posti);

      y = prei * 1000000 + sizeup(posti);
      //click_chatter("LONG: %s.%s %d",prep.c_str(),postp.c_str(), y);

      if ( args.size() > 2 ) {
        pointpos = args[2]. find_left('.',0);
        prep = args[2].substring(0, pointpos);
        postp = args[2].substring(pointpos+1,args[2].length());
        cp_integer(prep, &prei);
        cp_integer(postp, &posti);

        z = prei * 1000000 + sizeup(posti);
      //  click_chatter("HEIGHT: %s.%s %d",prep.c_str(),postp.c_str(), z);
      } else
        z = 0;

      pos->setGPSC(x,y,z);

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
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPS)
