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
 * uniquepacketsource.{cc,hh} - Creates unique packets
 *
 * Kurth M.
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet.hh>
#include "uniquepacketsource.hh"

CLICK_DECLS

UniquePacketSource::UniquePacketSource() : 
  _packet( NULL )
{
}

UniquePacketSource::~UniquePacketSource()
{
  if(_packet)
  {
    _packet->kill();
    _packet = NULL;
  }
}

int
UniquePacketSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int packet_size = 100;
  
  if (cp_va_parse(conf, this, errh,
      cpOptional,
        cpInteger, "packet size", &packet_size,
      cpKeywords,
        "SIZE", cpInteger, "packet size", &packet_size,
      cpEnd) < 0)
    return -1;

  WritablePacket *p = Packet::make(packet_size);
  _packet = p;

  void *data = p->data();
  memset( data, 'F', packet_size );

  return 0;
}

int
UniquePacketSource::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
UniquePacketSource::pull(int)
{
  Packet *p = _packet->clone();

  return( p );
}

EXPORT_ELEMENT(UniquePacketSource)
CLICK_ENDDECLS
