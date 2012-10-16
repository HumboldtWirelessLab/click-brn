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
 * timestampencap.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "timestampencap.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

TimestampEncap::TimestampEncap()
{
  BRNElement::init();
}

TimestampEncap::~TimestampEncap()
{
}

int
TimestampEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
TimestampEncap::smaction(Packet *p)
{
  if (WritablePacket *q = p->push(sizeof(struct timestamp_header))) {
    Timestamp *ts = &(q->timestamp_anno());
    struct timestamp_header *ts_header = (struct timestamp_header *)q->data();
    ts_header->tv_sec = htonl(ts->sec());
    ts_header->tv_usec = htonl(ts->nsec());

    return q;
  } else {
    p->kill();
    return 0;
  }
}

void
TimestampEncap::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
TimestampEncap::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

void
TimestampEncap::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BRNElement)
EXPORT_ELEMENT(TimestampEncap)
