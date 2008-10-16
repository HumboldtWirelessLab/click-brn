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
 * brntosclassifier.{cc,hh} -- classifies packets according to their brn tos field
 * M. Kurth
 */
 
#include <click/config.h>
#include "elements/brn/common.hh"

#include <clicknet/wifi.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "brntosclassifier.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnTosClassifier::BrnTosClassifier()
{
}

////////////////////////////////////////////////////////////////////////////////

BrnTosClassifier::~BrnTosClassifier()
{
}

////////////////////////////////////////////////////////////////////////////////


int 
BrnTosClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  
  _debug = 0;
  if (cp_va_kparse(conf, this, errh,
      /* not required */
      cpKeywords,
      "DEBUG", cpInteger, "Debug", &_debug,
      cpEnd) < 0)
    return -1;
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnTosClassifier::initialize(ErrorHandler *)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
BrnTosClassifier::push(int, Packet * p_in)
{
  BRN_CHECK_EXPR_RETURN(NULL == p_in,
    ("invalid arguments"), return);

//BRNNEW  int out_port = p_in->tos_anno();
  int out_port = 0;
  
  if (out_port >= noutputs())
  {
    BRN_WARN("Output %d not connected, discarding packet.", out_port);
  }
  
  BRN_DEBUG("sending packet with tos %d to output %d", out_port, out_port);
  checked_output_push(out_port, p_in);
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  BrnTosClassifier *td = (BrnTosClassifier *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BrnTosClassifier *f = (BrnTosClassifier *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnTosClassifier::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnTosClassifier)

////////////////////////////////////////////////////////////////////////////////
