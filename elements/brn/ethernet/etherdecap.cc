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
 * etherdecap.{cc,hh} -- removes Ethernet header from packets
 */

#include <click/config.h>
#include "etherdecap.hh"
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
CLICK_DECLS

EtherDecap::EtherDecap()
{
}

EtherDecap::~EtherDecap()
{
}

int
EtherDecap::configure(Vector<String> &, ErrorHandler *)
{
  return 0;
}


Packet *
EtherDecap::simple_action(Packet *p)
{
  click_ether *ether = (click_ether *)p->data();
  p->set_ether_header(ether);
  p->pull(sizeof(click_ether));

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EtherDecap)
