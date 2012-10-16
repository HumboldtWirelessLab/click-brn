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
 * gps_encap.{cc,hh} -- element removes (leading) BRN header at offset position 0.
 */

#include <click/config.h>
#include <click/confparse.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "gps_encap.hh"

CLICK_DECLS

GPSEncap::GPSEncap():
  _gps(NULL)
{
}

GPSEncap::~GPSEncap()
{
}

int
GPSEncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "GPS", cpkM+cpkP, cpElement, &_gps,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
GPSEncap::smaction(Packet *p)
{
  GPSPosition *pos = _gps->getPosition();

  WritablePacket *q = p->push(sizeof(struct gpsinfo_header));
  struct gpsinfo_header *gpsi = (struct gpsinfo_header *)q->data();
  gpsi->_lat = htonl(pos->_latitude.getPacketInt());
  gpsi->_long = htonl(pos->_longitude.getPacketInt());
  gpsi->_height = htonl(pos->_altitude.getPacketInt());
  gpsi->_speed = htonl(pos->_speed.getPacketInt());

  return q;
}

void
GPSEncap::push(int, Packet *p)
{
  if (Packet *q = smaction(p))
    output(0).push(q);
}

Packet *
GPSEncap::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GPSEncap)
ELEMENT_MT_SAFE(GPSEncap)
