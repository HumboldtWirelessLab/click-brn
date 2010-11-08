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
 * pushpacketheader.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/brn2.h"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "pushpacketheader.hh"

CLICK_DECLS

PushPacketHeader::PushPacketHeader()
  :_debug(BrnLogger::DEFAULT)
{
}

PushPacketHeader::~PushPacketHeader()
{
}

int
PushPacketHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _use_anno = false;

  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
push_packet_header(Packet *p) {
  WritablePacket *q;

  uint16_t pb = BRNPacketAnno::pulled_bytes_anno(p);
  BRNPacketAnno::set_pulled_bytes_anno(p,0);

  q = p->push(pb);

  return q;
}

void
PushPacketHeader::push(int, Packet *p)
{
  output(0).push(push_packet_header(p));
}

Packet *
PushPacketHeader::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return push_packet_header(p);
  } else {
    return 0;
  }
}


CLICK_ENDDECLS
EXPORT_ELEMENT(PushPacketHeader)
