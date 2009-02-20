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
 * printdeviceanno.{cc,hh} -- prints device annotations
 * M. Kurth
 */

#include <click/config.h>

#include "printdeviceanno.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnpacketanno.hh"
CLICK_DECLS

PrintDeviceAnno::PrintDeviceAnno()
{
}

PrintDeviceAnno::~PrintDeviceAnno()
{
}

Packet *
PrintDeviceAnno::simple_action(Packet *p_in)
{
  String dev_name(BRNPacketAnno::udevice_anno(p_in));

  // set device anno
  if (!(dev_name == "ath0" || dev_name == "eth0" || dev_name == "local")) {
    click_chatter(dev_name.c_str());
    assert(dev_name == "ath0" || dev_name == "eth0" || dev_name == "local");
  }

  return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintDeviceAnno)
