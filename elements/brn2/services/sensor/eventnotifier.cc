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

#include "eventpacket.hh"
#include "eventnotifier.hh"

CLICK_DECLS

EventNotifier::EventNotifier()
  :_payload_size(0),
   _timer(this),
   _handler_events(0),
   _push_packet_events(0),
   _pull_packet_events(0),
   _dht_storage(NULL),
   _id(0)
{
  BRNElement::init();
}

EventNotifier::~EventNotifier()
{
}

int
EventNotifier::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _eventhandleraddr = EtherAddress::make_broadcast();

  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM, cpEthernetAddress, &_me,
      "EVENTHANDLERADDR", cpkP, cpEthernetAddress, &_eventhandleraddr,
      "DEBUG", cpkP, cpInteger, &_debug,
      "DHTSTORAGE", cpkP, cpElement, &_dht_storage,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
EventNotifier::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  return 0;
}

void
EventNotifier::run_timer(Timer */*t*/)
{
  BRN_INFO("Trigger event");
  trigger_event(0);
}

void
EventNotifier::push( int /*port*/, Packet *packet )
{
  _push_packet_events++;
  trigger_event(0);
  checked_output_push(0, packet);
}

Packet *
EventNotifier::pull( int /*port*/ )
{
  _pull_packet_events++;
  trigger_event(0);
  return NULL;
}

void
EventNotifier::handle_event()
{
  if ( _dht_storage != NULL ) {
    BRN_INFO("Handle Event by DHT");
  };

  if ( noutputs() > 1 ) {
    BRN_INFO("Handle Event by output(1)");

    WritablePacket *p = WritablePacket::make(128 /*headroom*/,NULL /* *data*/, sizeof(struct event_header) + _payload_size, 32);
    struct event_header *eh = (struct event_header *)p->data();
    eh->_id = htons(_id++);
    memcpy(eh->_src, _me.data(),6);

    WritablePacket *p_out = BRNProtocol::add_brn_header(p, BRN_PORT_EVENTHANDLER, BRN_PORT_EVENTHANDLER,
                                                           DEFAULT_EVENT_MAX_HOP_COUNT, DEFAULT_TOS);
    BRNPacketAnno::set_ether_anno(p_out, _me, _eventhandleraddr, ETHERTYPE_BRN);
    checked_output_push(1, p_out);
  };
}


void
EventNotifier::trigger_event(int time)
{
  if ( time == 0 ) {
    BRN_INFO("Trigger Event");
    handle_event();
  } else {
    _timer.reschedule_after_msec(time);
  }
}




//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static int
write_payload_param(const String &in_s, Element *e, void *, ErrorHandler */*errh*/)
{
  EventNotifier *en = (EventNotifier *)e;
  String s = cp_uncomment(in_s);

  Vector<String> args;
  cp_spacevec(s, args);

  int payload_size;
  cp_integer(args[0], &payload_size);
  en->set_payload_size(payload_size);

  return 0;
}

static int
write_event_param(const String &in_s, Element *e, void *, ErrorHandler */*errh*/)
{
  EventNotifier *en = (EventNotifier *)e;
  String s = cp_uncomment(in_s);

  Vector<String> args;
  cp_spacevec(s, args);

  en->_handler_events++;

  if ( args[0].equals("now",strlen("now")) ) {
    en->trigger_event(0);
  } else {
    int time;
    cp_integer(args[0], &time);
    en->trigger_event(time);
  }

  return 0;
}

static String
read_stats_param(Element *e, void *)
{
  StringAccum sa;
  EventNotifier *en = (EventNotifier *)e;

  sa << "Events: " << (en->_push_packet_events + en->_pull_packet_events + en->_handler_events);
  sa << "\nPush Packets: " << en->_push_packet_events;
  sa << "\nPull Packets: " << en->_pull_packet_events;
  sa << "\nHandler Events: " << en->_handler_events;
  sa << "\n";
  return sa.take_string();
}

void
EventNotifier::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("event", write_event_param, 0);
  add_write_handler("payload_size", write_payload_param, 0);

  add_read_handler("stats", read_stats_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EventNotifier)
