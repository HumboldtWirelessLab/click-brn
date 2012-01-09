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
 * brnetherdecap.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "brn2_etherdecap.hh"

CLICK_DECLS

BRN2EtherDecap::BRN2EtherDecap()
{
  BRNElement::init();
}

BRN2EtherDecap::~BRN2EtherDecap()
{
}

int
BRN2EtherDecap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}

Packet *
BRN2EtherDecap::simple_action(Packet *p)
{
  click_ether *ether = (click_ether *)p->data();
  p->pull(sizeof(click_ether));

  p->set_ether_header(ether);

  BRNPacketAnno::set_ether_anno(p, EtherAddress(ether->ether_shost), EtherAddress(ether->ether_dhost), ntohs(ether->ether_type));

  return p;
}

void
BRN2EtherDecap::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2EtherDecap)
