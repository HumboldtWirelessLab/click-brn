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
 * nblist.{cc,hh} -- a list of information about neighbor nodes (brn-nodes)
 * A. Zubow
 */

#include <click/config.h>
//#include "elements/brn/common.hh"
#include "brn2_setdeviceanno.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
CLICK_DECLS

BRN2SetDeviceAnno::BRN2SetDeviceAnno()
// : _debug(BrnLogger::DEFAULT)
{
//  _nb_list = new NBMap();
}

BRN2SetDeviceAnno::~BRN2SetDeviceAnno()
{
//  BRN_DEBUG(" * current nb list: %s", printNeighbors().c_str());

//  delete _nb_list;
}

int
BRN2SetDeviceAnno::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEVICE", cpkP+cpkM, cpElement, &_device,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BRN2SetDeviceAnno::initialize(ErrorHandler *)
{
  return 0;
}


Packet *
BRN2SetDeviceAnno::simple_action(Packet *p_in)
{
  BRNPacketAnno::set_devicenumber_anno(p_in, _device->getDeviceNumber());
  return p_in;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element */*e*/, void *)
{
//  BRN2SetDeviceAnno *da = (BRN2SetDeviceAnno *)e;
  return /*String(nl->_debug) + */"not supported\n";
}

static int 
write_debug_param(const String &in_s, Element */*e*/, void *, ErrorHandler *errh)
{
//  BRN2SetDeviceAnno *da = (BRN2SetDeviceAnno *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  //da->_debug = debug;
  return 0;
}

void
BRN2SetDeviceAnno::add_handlers()
{
   add_read_handler("debug", read_debug_param, 0);

//  add_write_handler("insert", static_insert, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2SetDeviceAnno)
