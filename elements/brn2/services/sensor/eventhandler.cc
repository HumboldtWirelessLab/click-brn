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
 * eventnotifier.{cc,hh}
 */

#include <click/config.h>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "eventhandler.hh"

CLICK_DECLS

EventHandler::EventHandler()
  :_debug(BrnLogger::DEFAULT)
{
}

EventHandler::~EventHandler()
{
}

int
EventHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
EventHandler::initialize(ErrorHandler *)
{
  return 0;
}

void
EventHandler::push( int /*port*/, Packet *packet )
{
  packet->kill();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_stats_param(Element *e, void *)
{
  StringAccum sa;
  EventHandler *eh = (EventHandler *)e;

  sa << "Events: " << (eh->_packet_events + eh->_dht_events);
  sa << "\nPacket events: " << eh->_packet_events;
  sa << "\nDHT events: " << eh->_dht_events;
  sa << "\n";

  return sa.take_string();
}

static String
read_debug_param(Element *e, void *)
{
  EventHandler *fl = (EventHandler *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  EventHandler *fl = (EventHandler *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
EventHandler::add_handlers()
{
  add_read_handler("stats", read_stats_param, 0);
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EventHandler)
