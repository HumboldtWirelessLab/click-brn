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
 * brn2_setdeviceanno.{cc,hh}
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "brn2_setdeviceanno.hh"

CLICK_DECLS

BRN2SetDeviceAnno::BRN2SetDeviceAnno()
:_device(NULL),_device_number(0){
  BRNElement::init();
}

BRN2SetDeviceAnno::~BRN2SetDeviceAnno()
{
}

int
BRN2SetDeviceAnno::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEVICE", cpkP+cpkM, cpElement, &_device,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BRN2SetDeviceAnno::initialize(ErrorHandler *)
{
  _device_number = _device->getDeviceNumber();

  return 0;
}


Packet *
BRN2SetDeviceAnno::simple_action(Packet *p_in)
{
  BRNPacketAnno::set_devicenumber_anno(p_in, _device_number);
  return p_in;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRN2SetDeviceAnno::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SetDeviceAnno)
