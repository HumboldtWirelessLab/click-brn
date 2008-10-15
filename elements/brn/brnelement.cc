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
#include "brnelement.hh"

#include <click/confparse.hh>

CLICK_DECLS

BRNElement::BRNElement() :
  _debug(BrnLogger::DEFAULT)
{
}

BRNElement::~BRNElement()
{
}

enum {H_DEBUG};

static String 
read_debug(Element *e, void *thunk)
{
  BRNElement *td = (BRNElement *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_debug(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BRNElement *f = (BRNElement *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {    //debug
      int debug;
//BRNNEW 
/*      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be integer");*/
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
  add_read_handler("debug", read_debug, (void *) H_DEBUG);
  add_write_handler("debug", write_debug, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

ELEMENT_REQUIRES(brn_common)
ELEMENT_PROVIDES(BRNElement)
CLICK_ENDDECLS
