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

#if CLICK_NS
#include <click/router.hh>
#endif

#include "brnelement.hh"

CLICK_DECLS

int BRNElement::_ref_counter = 0;
bool BRNElement::_init_rand = false;

BRNElement::BRNElement() :
  _debug(BrnLogger::DEFAULT)
{
  _ref_counter++;
}

BRNElement::~BRNElement()
{
  _ref_counter--;
  if ( _ref_counter == 0 ) {
    BrnLogger::destroy();
  }
}

void
BRNElement::init(void)
{
  _debug = BrnLogger::DEFAULT;
}

void
BRNElement::click_brn_srandom(void)
{
  if (!_init_rand) {
#if CLICK_NS
    uint32_t init_seed = 0;
    simclick_sim_command(router()->simnode(), SIMCLICK_GET_RANDOM_INT, &init_seed, (uint32_t)0x9FFFFFFF);
    click_srandom(init_seed);
    srand(init_seed);
#else
    click_random_srandom();
#endif
    _init_rand = true;
  }
}

String
BRNElement::get_node_name()
{
  return BRN_NODE_NAME;
}

enum {H_DEBUG};

static String
read_brnelement(Element *e, void *thunk)
{
  BRNElement *td = reinterpret_cast<BRNElement *>(e);
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int
write_brnelement(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BRNElement *f = reinterpret_cast<BRNElement *>(e);
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

  add_write_handler("debug", write_brnelement, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNElement)

