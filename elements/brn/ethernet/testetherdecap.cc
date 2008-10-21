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
 * TestEtherDecap.{cc,hh} -- removes Ethernet header from packets
 */

#include <click/config.h>
#include "testetherdecap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include "elements/brn/standard/brnpacketanno.hh"

CLICK_DECLS

TestEtherDecap::TestEtherDecap()
{
}

TestEtherDecap::~TestEtherDecap()
{
}

int
TestEtherDecap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
TestEtherDecap::smaction(Packet *p)
{
  //click_ether *ether = (click_ether *)p->ether_header();

  //if (!ether) {
    click_ether *ether = (click_ether *)p->data();
  //}

  EtherAddress dst(ether->ether_dhost);
  EtherAddress src(ether->ether_shost);
  uint16_t ether_type = ether->ether_type;

  WritablePacket *p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }

  p_out = p->push_mac_header(14);
  if (!p_out) {
    return 0;
  }

  memcpy(p_out->data(), dst.data(), 6);
  memcpy(p_out->data() + 6, src.data(), 6);
  memcpy(p_out->data() + 12, &ether_type, 2);

  p_out->pull(sizeof(click_ether));

  //save packet anno
  BRNPacketAnno::set_udevice_anno(p_out,(BRNPacketAnno::udevice_anno(p)).c_str());

  return p_out;
}

void
TestEtherDecap::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
TestEtherDecap::pull(int)
{
  if (Packet *p = input(0).pull())
    return smaction(p);
  else
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TestEtherDecap)
