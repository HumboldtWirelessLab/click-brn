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
 * gps_print.{cc,hh} -- element removes (leading) BRN header at offset position 0.
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "gps.hh"
#include "gps_info.hh"
#include "gps_print.hh"

CLICK_DECLS

GPSPrint::GPSPrint():
  _nowrap(false),
  _oldgps(false)
{
}

GPSPrint::~GPSPrint()
{
}

int
GPSPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NOWRAP", cpkP, cpBool, &_nowrap,
      "DEBUG", cpkP, cpInteger, &_debug,
      "OLDGPS", cpkP, cpBool, &_oldgps,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
GPSPrint::smaction(Packet *p)
{
  struct gpsinfo_header *gpsi = (struct gpsinfo_header *)p->data();

  GPSPosition pos;

  if ( _oldgps ) {
    pos = GPSPosition(FixPointNumber(gpsi->_lat),
                      FixPointNumber(gpsi->_long),
                      FixPointNumber(gpsi->_height));

    pos.setSpeed(0);
  } else {
    pos = GPSPosition(FixPointNumber(ntohl(gpsi->_lat)),
                      FixPointNumber(ntohl(gpsi->_long)),
                      FixPointNumber(ntohl(gpsi->_height)));

    pos.setSpeed(ntohl(gpsi->_speed));
  }

  StringAccum sa;
  sa << "LAT: " << pos._latitude.unparse() << " ";
  sa << "LONG: " << pos._longitude.unparse() << " ";
  sa << "ALT: " << pos._altitude.unparse() << " ";
  sa << "SPEED: " << pos._speed.unparse();

  if ( ! _nowrap ) {
    click_chatter("%s",sa.take_string().c_str());
  } else {
    BrnLogger::chatter("%s ",sa.take_string().c_str());
  }

  return p;
}

void
GPSPrint::push(int, Packet *p)
{
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
GPSPrint::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPSPrint)
ELEMENT_MT_SAFE(GPSPrint)
