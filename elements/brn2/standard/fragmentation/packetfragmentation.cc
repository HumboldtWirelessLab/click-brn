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
 * PacketFragmentation.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "packetfragmentation.hh"

CLICK_DECLS

PacketFragmentation::PacketFragmentation():
  _max_packet_size(1492),
  _packet_id(0)
{
  BRNElement::init();
}

PacketFragmentation::~PacketFragmentation()
{
}

int
PacketFragmentation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "MAXSIZE", cpkP+cpkM, cpUnsigned, &_max_packet_size,
      "DEBUG", cpkP, cpUnsigned, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
PacketFragmentation::initialize(ErrorHandler *)
{
  return 0;
}

void
PacketFragmentation::push( int /*port*/, Packet *packet )
{
  WritablePacket *p = packet->uniqueify();
  WritablePacket *p_frag;
  struct fragmention_header *frag_h;

  if ( p->length() > _max_packet_size ) {
    _packet_id++;

    int no_fragments = p->length() / PACKET_FRAGMENT_SIZE;

    int _next_fragment_size = (p->length() % PACKET_FRAGMENT_SIZE);
    if ( _next_fragment_size > 0 ) no_fragments++;

    if ( no_fragments > MAX_FRAGMENTS ) {
      BRN_ERROR("Too many fragments (%d). Kill packet.",no_fragments);
      p->kill();
      return;
    }

    int no_frag_overall = no_fragments;

    while ( no_fragments > 0 ) {
      _next_fragment_size = (p->length() % PACKET_FRAGMENT_SIZE);
      if ( _next_fragment_size == 0 ) _next_fragment_size = PACKET_FRAGMENT_SIZE;

      if ( no_fragments > 1 ) {
        p_frag = WritablePacket::make( 256, &((p->data())[(no_fragments-1)*PACKET_FRAGMENT_SIZE]), _next_fragment_size, 32);
        p->take(_next_fragment_size);
        p_frag->copy_annotations(p);
      } else {
        p_frag = p;
      }

      WritablePacket *p_frag_h = p_frag->push(sizeof(struct fragmention_header));

      frag_h = (struct fragmention_header *)p_frag_h->data();
      frag_h->packet_id = htons(_packet_id);
      frag_h->no_fragments = no_frag_overall;
      frag_h->fragment_id = (--no_fragments);

      WritablePacket *p_out = BRNProtocol::add_brn_header(p_frag_h, BRN_PORT_FRAGMENTATION, BRN_PORT_FRAGMENTATION);

      output(0).push(p_out);
    }
  } else {
    if ( noutputs() > 1 ) {
      output(1).push(packet);
    } else {
      output(0).push(packet);
    }
  }
}

void
PacketFragmentation::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketFragmentation)
