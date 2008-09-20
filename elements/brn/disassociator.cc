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
 * disassocitor.{cc,hh} -- Send dissacociations to unassociated clients
 *
 * A. Zubow
 */

#include <click/config.h>
#include "common.hh"

#include "disassociator.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brnassocresponder.hh"
#include "brniappstationtracker.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

Disassociator::Disassociator()
  : _debug(BrnLogger::DEFAULT),
    _sta_tracker(NULL),
    _assocresp(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

Disassociator::~Disassociator()
{
}

////////////////////////////////////////////////////////////////////////////////

int
Disassociator::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_parse(conf, this, errh,
        cpKeywords,
        //"Ethernet", cpEtherAddress, "Ethernet address", &_ether,
        "ASSOC_RESP", cpElement, "BRNAssocResponder", &_assocresp,
        "STA_TRACKER", cpElement, "BrnIappStationTracker", &_sta_tracker,
        cpEnd) < 0)
    return -1;

  if (!_assocresp || !_assocresp->cast("BRNAssocResponder")) 
    return errh->error("BRNAssocResponder not specified");

  if (!_sta_tracker || !_sta_tracker->cast("BrnIappStationTracker")) 
    return errh->error("BrnIappStationTracker not specified");
    
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
Disassociator::initialize(ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

/* should be an annotated brn packet */
Packet *
Disassociator::simple_action(Packet *p_in)
{
  const click_ether *ether = (click_ether *)p_in->data();

  if (!ether) {
    BRN_WARN("Disassociator got packet without valid ether!");
    return (p_in);
  }

  EtherAddress src_addr(ether->ether_shost);
  String device(p_in->udevice_anno());

  // client is not associated
  if (!_sta_tracker->update(src_addr)) {
    // TODO test if station is wireless station
    BRN_INFO(" Station %s is not associated. Sending dissassoc.", src_addr.s().c_str());
    _assocresp->send_disassociation(src_addr, WIFI_REASON_NOT_ASSOCED);
    
    p_in->kill();
    return NULL;
  }

  BRN_DEBUG(" Station %s is associated (refresh assoc list with dev %s).", 
      src_addr.s().c_str(), device.c_str());

  return p_in;
}

////////////////////////////////////////////////////////////////////////////////

static String
read_debug_param(Element *e, void *thunk)
{
  UNREFERENCED_PARAMETER(thunk);

  Disassociator *al = (Disassociator *)e;
  return String(al->_debug) + "\n";
}

////////////////////////////////////////////////////////////////////////////////

static int 
write_debug_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(vparam);

  Disassociator *al = (Disassociator *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be boolean");
  al->_debug = debug;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
Disassociator::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

////////////////////////////////////////////////////////////////////////////////

#include <click/bighashmap.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(Disassociator)

////////////////////////////////////////////////////////////////////////////////
