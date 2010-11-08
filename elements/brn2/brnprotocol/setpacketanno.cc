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
 * setpacketanno.{cc,hh} -- prepends a brn header
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "setpacketanno.hh"

CLICK_DECLS

SetPacketAnno::SetPacketAnno()
  : _ttl(-1),
    _tos(-1),
    _debug(BrnLogger::DEFAULT)
{
}

SetPacketAnno::~SetPacketAnno()
{
}

int
SetPacketAnno::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "TTL", cpkP, cpInteger, &_ttl,
      "TOS", cpkP, cpInteger, &_tos,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
SetPacketAnno::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
SetPacketAnno::smaction(Packet *p)
{

  if ( _ttl > -1 ) BRNPacketAnno::set_ttl_anno(p, _ttl);
  if ( _tos > -1 ) BRNPacketAnno::set_tos_anno(p, _tos);

  return p;
}

void
SetPacketAnno::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
SetPacketAnno::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

/*****************************************************************************/
/******************************* H A N D L E R *******************************/
/*****************************************************************************/

static String
read_debug_param(Element *e, void *)
{
  SetPacketAnno *be = (SetPacketAnno *)e;
  return String(be->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  SetPacketAnno *be = (SetPacketAnno *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  be->_debug = debug;
  return 0;
}

void
SetPacketAnno::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetPacketAnno)
