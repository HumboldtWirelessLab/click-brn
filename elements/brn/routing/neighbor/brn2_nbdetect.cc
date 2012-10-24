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
 * nblist.{cc,hh} -- a list of information about neighbor nodes (brn-nodes)
 * A. Zubow
 */

#include <click/config.h>
#include "brn2_nbdetect.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

NeighborDetect::NeighborDetect()
{
  BRNElement::init();
}

NeighborDetect::~NeighborDetect()
{
}

int
NeighborDetect::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NBLIST", cpkP+cpkM, cpElement, &_nblist,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
NeighborDetect::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
NeighborDetect::simple_action(Packet *p_in)
{
  const click_ether *ether = (click_ether *)p_in->ether_header();

  if (ether) _nblist->insert(EtherAddress(ether->ether_shost), BRNPacketAnno::devicenumber_anno(p_in));

  return p_in;
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
NeighborDetect::add_handlers()
{
  BRNElement::add_handlers();
}

#include <click/bighashmap.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(NeighborDetect)
