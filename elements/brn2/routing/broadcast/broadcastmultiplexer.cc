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
 * broadcastmultiplexer.{cc,hh} -- copy packet for each device and set src_mac
 * R. Sombrutzki
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "broadcastmultiplexer.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

BroadcastMultiplexer::BroadcastMultiplexer()
  :_me(NULL)
{
}

BroadcastMultiplexer::~BroadcastMultiplexer()
{
}

int
BroadcastMultiplexer::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _use_anno = false;

  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "USEANNO", cpkP, cpBool, &_use_anno,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("BRN2NodeIdentity not specified");

  return 0;
}

int
BroadcastMultiplexer::initialize(ErrorHandler *)
{
  return 0;
}

void
BroadcastMultiplexer::uninitialize()
{
  //cleanup
}

/* Processes an incoming (brn-)packet. */
void
BroadcastMultiplexer::push(int /*port*/, Packet *p_in)
{
  const EtherAddress *ea;

  for ( int i = 1; i < _me->countDevices(); i++) {
    ea = _me->getDeviceByIndex(i)->getEtherAddress();
    Packet *p_copy = p_in->clone();

    if ( _use_anno ) {
      BRNPacketAnno::set_src_ether_anno(p_copy, *ea);
    } else {
      click_ether *ether = (click_ether *) p_copy->data();

      memcpy(ether->ether_shost,ea->data(),6);
    }
    output(0).push(p_copy);
  }

  if ( _me->countDevices() > 0 ) {
    ea = _me->getDeviceByIndex(0)->getEtherAddress();

    if ( _use_anno ) {
      BRNPacketAnno::set_src_ether_anno(p_in, *ea);
    } else {
      click_ether *ether = (click_ether *) p_in->data();

       memcpy(ether->ether_shost,ea->data(),6);
    }

    output(0).push(p_in);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BroadcastMultiplexer *ds = (BroadcastMultiplexer *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BroadcastMultiplexer *ds = (BroadcastMultiplexer *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
BroadcastMultiplexer::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(BroadcastMultiplexer)

CLICK_ENDDECLS
