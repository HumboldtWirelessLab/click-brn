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
 * timestampdecap.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timestamp.hh>
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "timestampencap.hh"

#include "timestampdecap.hh"


CLICK_DECLS

TimestampDecap::TimestampDecap()
{
  BRNElement::init();
}

TimestampDecap::~TimestampDecap()
{
}

int
TimestampDecap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
TimestampDecap::simple_action(Packet *p)
{
  struct timestamp_header *ts_header = (struct timestamp_header *)p->data();

  p->timestamp_anno().assign(ntohl(ts_header->tv_sec), ntohl(ts_header->tv_usec));

  p->pull(sizeof(struct timestamp_header));

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TimestampDecap)
