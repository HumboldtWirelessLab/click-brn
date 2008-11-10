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
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn/wifi/brnwifi.h>

#include "airtimeestimation.hh"

CLICK_DECLS

AirTimeEstimation::AirTimeEstimation()
{
}

AirTimeEstimation::~AirTimeEstimation()
{
}

int
AirTimeEstimation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _debug = false;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpBool, &_debug,
                     cpEnd);
  return ret;
}

Packet *
AirTimeEstimation::simple_action(Packet *p)
{
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AirTimeEstimation)
ELEMENT_MT_SAFE(AirTimeEstimation)
