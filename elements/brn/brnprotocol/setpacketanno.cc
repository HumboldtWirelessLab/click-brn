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
    _tos(-1)
{
  BRNElement::init();
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
      "QUEUE", cpkP, cpInteger, &_queue,
      "TOS2QUEUE", cpkP, cpBool, &_queue_like_tos,
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

void
SetPacketAnno::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetPacketAnno)
