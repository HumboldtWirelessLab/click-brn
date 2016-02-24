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
 * brnds.{cc,hh} -- forwards brn packets to the right device
 * A. Zubow
 */

#include <click/config.h>

#include "brn2_ds.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

CLICK_DECLS

BRN2DS::BRN2DS()
  : _debug(0), _me(NULL), countDevices(0)
{
}

BRN2DS::~BRN2DS()
{
}

int
BRN2DS::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, /*"NodeIdentity",*/ &_me,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("BRN2NodeIdentity not specified");

  countDevices = _me->countDevices();

  return 0;
}

int
BRN2DS::initialize(ErrorHandler *)
{
  return 0;
}

void
BRN2DS::uninitialize()
{
  //cleanup
}

/* Processes an incoming (brn-)packet. */
void
BRN2DS::push(int /*port*/, Packet *p_in)
{
  const click_ether *ether = reinterpret_cast<const click_ether *>( p_in->data());

  EtherAddress ea = EtherAddress(ether->ether_shost);
  int outputport = _me->getDeviceNumber(&ea);

  if ( outputport >= 0 )
    output(outputport).push(p_in);
  else
    output(countDevices).push(p_in);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2DS *ds = reinterpret_cast<BRN2DS *>(e);
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2DS *ds = reinterpret_cast<BRN2DS *>(e);
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
BRN2DS::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(BRN2DS)

CLICK_ENDDECLS
