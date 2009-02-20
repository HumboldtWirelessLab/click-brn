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
 * brnetherencap.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include "brnetherencap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brn.h"


CLICK_DECLS

BRNEtherEncap::BRNEtherEncap()
{
}

BRNEtherEncap::~BRNEtherEncap()
{
}

int
BRNEtherEncap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
BRNEtherEncap::smaction(Packet *p)
{
  if (WritablePacket *q = p->push(14)) {

    click_ether *ether = (click_ether *) q->data();
    click_ether *annotated_ether = (click_ether *)p->ether_header();

    if (annotated_ether) {  // are the mac annotations available?

      memcpy(ether->ether_shost, annotated_ether->ether_shost, 6);
      memcpy(ether->ether_dhost, annotated_ether->ether_dhost, 6);
      ether->ether_type = annotated_ether->ether_type;

      //debug  //TODO: Remove this
      if ( memcmp((BRNPacketAnno::dst_ether_anno(p)).data(),annotated_ether->ether_dhost,6) != 0 ) {
        click_chatter("header is set, but DST Addresses differs");
        click_chatter("Header: %s  Anno: %s",
                       EtherAddress(annotated_ether->ether_dhost).unparse().c_str(),
                       (BRNPacketAnno::dst_ether_anno(p)).unparse().c_str());
      }

    } else {

      click_chatter("The mac header anno isn't set. Use annos\n");
      memcpy(ether->ether_shost, (BRNPacketAnno::src_ether_anno(p)).data(), 6);
      memcpy(ether->ether_dhost, (BRNPacketAnno::dst_ether_anno(p)).data(), 6);
      ether->ether_type = htons(BRNPacketAnno::ethertype_anno(p));

    }

    q->set_ether_header(ether);
    return q;

  } else {
    p->kill();
    return 0;
  }
}

void
BRNEtherEncap::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
BRNEtherEncap::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNEtherEncap)
