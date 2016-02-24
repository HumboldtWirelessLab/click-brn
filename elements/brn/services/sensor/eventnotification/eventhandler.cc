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

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "eventpacket.hh"
#include "eventhandler.hh"

CLICK_DECLS

EventHandler::EventHandler()
  :_packet_events(0),
   _dht_events(0)
{
  BRNElement::init();
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
  if ( packet->length() >= sizeof(struct event_header) ) {
    struct event_header *eh = (struct event_header *)packet->data();
    EtherAddress ea = EtherAddress(eh->_src);

    int distance = DEFAULT_EVENT_MAX_HOP_COUNT - BRNPacketAnno::ttl_anno(packet);
    if ( (distance > DEFAULT_EVENT_MAX_HOP_COUNT) || (distance < 0) ) distance = -1;

    if ( ! contains_event(ea, ntohs(eh->_id)) ) {
      BRN_INFO("Add new event: %s:%d",ea.unparse().c_str(), ntohs(eh->_id) );
      _packet_events++;
      add_event(ea, ntohs(eh->_id), distance);
    } else {
      BRN_INFO("Already known event: %s:%d",ea.unparse().c_str(), ntohs(eh->_id));
    }
  }

  packet->kill();
}

/*
void
EventHandler::clear_eventlist()
{
  _evli.clear();
}
*/

void
EventHandler::add_event(EtherAddress src, int id, int distance)
{
  _evli.push_back(EventHandler::DetectedEvent(src,id,distance));
}

bool
EventHandler::contains_event(EtherAddress src, int id)
{
  for ( int i = 0; i < _evli.size(); i++ ) {
    EventHandler::DetectedEvent *ev = &_evli[i];
    if ( ( src == ev->_src) && ( id == ev->_id ) ) return true;
  }

  return false;
}


String
EventHandler::get_events()
{
  StringAccum sa;

  sa << "<event node=\"" << BRN_NODE_NAME << "\" count=\"" << (_packet_events + _dht_events) << "\" >\n";
  sa << "\t<packet_event count=\"" << _packet_events << "\" >\n" ;
  for ( int i = 0; i < _evli.size(); i++ ) {
    EventHandler::DetectedEvent *ev = &_evli[i];
    sa << "\t\t<src addr=\"" << ev->_src.unparse() << "\" id=\"" << ev->_id << "\" distance=\"" << ev->_distance << "\" />\n";
  }
  sa << "\t</packet_event>\n";
  sa << "\t<dht_event count=\"" << _dht_events << "\" ></dht_event>\n";
  sa << "</event>\n";

  return sa.take_string();
}

void
EventHandler::reset()
{
  _packet_events = _dht_events = 0;
  _evli.clear();
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_stats_param(Element *e, void *)
{
  EventHandler *eh = reinterpret_cast<EventHandler *>(e);

  return eh->get_events();
}

static int
write_reset_param(const String &/*in_s*/, Element *e, void *, ErrorHandler */*errh*/)
{
  EventHandler *eh = reinterpret_cast<EventHandler *>(e);
  eh->reset();

  return 0;
}

void
EventHandler::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);

  add_write_handler("reset", write_reset_param, 0);
}

#include <click/vector.cc>
template class Vector<EventHandler::DetectedEvent>;

CLICK_ENDDECLS
EXPORT_ELEMENT(EventHandler)
