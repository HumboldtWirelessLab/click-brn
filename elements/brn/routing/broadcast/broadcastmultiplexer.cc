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
#include "elements/brn/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

BroadcastMultiplexer::BroadcastMultiplexer()
  :_me(NULL),
   _use_anno(false)
{
  BRNElement::init();
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
      "DEBUG", cpkP, cpInteger, &_debug,
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

  EtherAddress src;

  if ( _use_anno )
    src = EtherAddress(BRNPacketAnno::src_ether_anno(p_in));
  else
    src = EtherAddress(((click_ether *)p_in->data())->ether_shost);

  if ( ! src.is_broadcast() ) {  //src address looks valid, so no need to send it on all devices
    output(0).push(p_in);
  } else {                       //forward packet using all devices which allow broadcast
    int f;
    BRN2Device *first_device = NULL;

    for ( f = 0; f < _me->countDevices(); f++) {
      if ( _me->getDeviceByIndex(f)->allow_broadcast() ) {
        first_device = _me->getDeviceByIndex(f);
        break;
      }
    }

    for ( int i = f+1; i < _me->countDevices(); i++) {
      if ( _me->getDeviceByIndex(i)->allow_broadcast() ) {

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
    }

    if ( first_device ) {
      ea = first_device->getEtherAddress();

      if ( _use_anno ) {
        BRNPacketAnno::set_src_ether_anno(p_in, *ea);
      } else {
        click_ether *ether = (click_ether *) p_in->data();

        memcpy(ether->ether_shost,ea->data(),6);
      }

      output(0).push(p_in);
    }
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BroadcastMultiplexer::add_handlers()
{
  BRNElement::add_handlers();
}

EXPORT_ELEMENT(BroadcastMultiplexer)

CLICK_ENDDECLS