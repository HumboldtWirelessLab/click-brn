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
 * setbrntos.{cc,hh} -- sets the tos annotation
 * M. Kurth
 */
 
#include <click/config.h>
#include "elements/brn/common.hh"

#include <clicknet/wifi.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "setbrntos.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

SetBrnTos::SetBrnTos()
{
}

////////////////////////////////////////////////////////////////////////////////

SetBrnTos::~SetBrnTos()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
SetBrnTos::configure(Vector<String> &conf, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  
  _debug = 0;
  _tos = BRN_TOS_BE;
  if (cp_va_kparse(conf, this, errh,
      cpUnsigned, "TOS to set", &_tos,
      /* not required */
      cpKeywords,
      "DEBUG", cpInteger, "Debug", &_debug,
      cpEnd) < 0)
    return -1;
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
SetBrnTos::initialize(ErrorHandler *)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

Packet * 
SetBrnTos::simple_action(Packet *p_in)
{
  BRN_CHECK_EXPR_RETURN(NULL == p_in,
    ("invalid arguments"), if (p_in) p_in->kill();return(NULL););
  
 // p_in->set_tos_anno(_tos);
  
  return (p_in);
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  SetBrnTos *td = (SetBrnTos *)e;
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
  SetBrnTos *f = (SetBrnTos *)e;
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
SetBrnTos::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(SetBrnTos)

////////////////////////////////////////////////////////////////////////////////
