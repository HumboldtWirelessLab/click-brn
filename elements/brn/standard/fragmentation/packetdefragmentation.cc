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
 * PacketDefragmentation.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "packetfragmentation.hh"
#include "packetdefragmentation.hh"

CLICK_DECLS

PacketDefragmentation::PacketDefragmentation()
{
  BRNElement::init();
}

PacketDefragmentation::~PacketDefragmentation()
{
}

int
PacketDefragmentation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpUnsigned, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
PacketDefragmentation::initialize(ErrorHandler *)
{
  return 0;
}

void
PacketDefragmentation::push( int /*port*/, Packet *packet )
{
  click_ether *annotated_ether = (click_ether *)packet->ether_header();
  EtherAddress src = EtherAddress(annotated_ether->ether_shost);

  struct fragmention_header *fh = (struct fragmention_header*)packet->data();
  uint16_t packet_id = ntohs(fh->packet_id);

  BRN_DEBUG("PacketID: %d Src: %s", packet_id, src.unparse().c_str());

  SrcInfo *src_i = _src_tab.findp(src);

  if ( src_i == NULL ) {
    BRN_DEBUG("New src");
    _src_tab.insert(src,SrcInfo(&src));
    src_i = _src_tab.findp(src);
  }

  FragmentationInfo *fi = src_i->getFragmentionInfo(packet_id);

  if ( fi == NULL ) {
    BRN_DEBUG("New fi");
    fi = src_i->getLRUFragmentionInfo(true);
  }
  BRN_DEBUG("insert");
  fi->insert_packet(packet);

  if ( fi->is_complete() ) {
    BRN_DEBUG("finished");
    output(0).push(fi->p);
    fi->p = NULL;
    fi->clear();
  }
}

void
PacketDefragmentation::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketDefragmentation)
