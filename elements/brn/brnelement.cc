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

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>

#include "brnelement.hh"

CLICK_DECLS

PacketPool *BRNElement::_packet_pool = NULL;
int BRNElement::_ref_counter = 0;

BRNElement::BRNElement() :
  _debug(BrnLogger::DEFAULT)
{
  if ( _packet_pool == NULL ) {
    _ref_counter = 1;
    _packet_pool = new PacketPool(PACKET_POOL_CAPACITY, PACKET_POOL_SIZE_STEPS,
                                  PACKET_POOL_MIN_SIZE, PACKET_POOL_MAX_SIZE, DEFAULT_HEADROOM, DEFAULT_TAILROOM);
  } else {
    _ref_counter++;
  }
}

BRNElement::~BRNElement()
{
  _ref_counter--;
  if ( _ref_counter == 0 ) {
    if ( _packet_pool ) {
      delete _packet_pool;
      _packet_pool = NULL;
    }

    BrnLogger::destroy();
  }
}

void
BRNElement::init(void)
{
  _debug = BrnLogger::DEFAULT;
}

String
BRNElement::get_node_name()
{
  return BRN_NODE_NAME;
}  

void
BRNElement::packet_kill(Packet *p)
{
  if ( BRNElement::_packet_pool ) {
    BRNElement::_packet_pool->insert(p);
  } else {
    p->kill();
  }
}

WritablePacket *
BRNElement::packet_new(uint32_t headroom, uint8_t *data, uint32_t size, uint32_t tailroom)
{
  if ( BRNElement::_packet_pool ) {
    return BRNElement::_packet_pool->get(headroom, data, size, tailroom);
  } else {
    return WritablePacket::make(headroom, data, size, tailroom);
  }
}

enum {H_DEBUG, H_PACKETPOOL};

static String
read_brnelement(Element *e, void *thunk)
{
  BRNElement *td = (BRNElement *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_PACKETPOOL:
    if ( BRNElement::_packet_pool ) {
      return BRNElement::_packet_pool->stats();
    } else {
      return String();
    }
  default:
    return String();
  }
}

static int
write_brnelement(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BRNElement *f = (BRNElement *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {    //debug
      int debug;

      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be integer");
      f->_debug = debug;
      break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
BRNElement::add_handlers()
{
  add_read_handler("debug", read_brnelement, (void *) H_DEBUG);
  add_read_handler("packetpool", read_brnelement, (void *) H_PACKETPOOL);

  add_write_handler("debug", write_brnelement, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNElement)

