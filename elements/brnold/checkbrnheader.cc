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
 * checkbrnheader.{cc,hh} -- checks brn packets, sets the tos annotation
 * M. Kurth
 */
 
#include <click/config.h>
#include "common.hh"

#include <clicknet/wifi.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "checkbrnheader.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

CheckBrnHeader::CheckBrnHeader()
{
}

////////////////////////////////////////////////////////////////////////////////

CheckBrnHeader::~CheckBrnHeader()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
CheckBrnHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  
  _debug = 0;
  if (cp_va_parse(conf, this, errh,
      /* not required */
      cpKeywords,
      "DEBUG", cpInteger, "Debug", &_debug,
      cpEnd) < 0)
    return -1;
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
CheckBrnHeader::initialize(ErrorHandler *)
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

Packet * 
CheckBrnHeader::simple_action(Packet *p_in)
{
  BRN_CHECK_EXPR_RETURN(NULL == p_in || p_in->length() < sizeof(click_brn),
    ("invalid arguments"), if (p_in) p_in->kill();return(NULL););
  
  click_brn* pBrn = (click_brn*) p_in->data();
  
  p_in->set_tos_anno(pBrn->tos);
  
  if (BRN_TOS_BE != pBrn->tos)
  {
    BRN_DEBUG("seen frame with tos %d for port %d", pBrn->tos, pBrn->dst_port);
  }
  
  return (p_in);
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  CheckBrnHeader *td = (CheckBrnHeader *)e;
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
  CheckBrnHeader *f = (CheckBrnHeader *)e;
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
CheckBrnHeader::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckBrnHeader)

////////////////////////////////////////////////////////////////////////////////
