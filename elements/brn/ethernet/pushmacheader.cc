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
 * pushmacheader.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include "pushmacheader.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

PushMACHeader::PushMACHeader()
{
}

PushMACHeader::~PushMACHeader()
{
}

int
PushMACHeader::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
PushMACHeader::smaction(Packet *p)
{
  const click_ether *ether_anno = (const click_ether *)p->ether_header();
  EtherAddress src_addr(ether_anno->ether_shost);
  EtherAddress dst_addr(ether_anno->ether_dhost);
  if (WritablePacket *q = p->push(14)) {
    click_ether *ether = (click_ether *) q->data();
    memcpy(ether->ether_shost, src_addr.data(), 6);
    memcpy(ether->ether_dhost, dst_addr.data(), 6);
    ether->ether_type = ether_anno->ether_type;
    return q;
  } else {
    return 0;
  }
}

void
PushMACHeader::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
PushMACHeader::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PushMACHeader)
