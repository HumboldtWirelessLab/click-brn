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
 * dnsclient.{cc,hh} -- responds to dhcp requests
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "dnsprotocol.hh"
#include "brn2_dnsclient.hh"


CLICK_DECLS

BRN2DNSClient::BRN2DNSClient()
  : _interval(0),
    _start_time(0),
    _timer(this),
    transid(0),_active(false),
    _running_request(false),
    _no_requests(0),
    _no_replies(0),
    _sum_request_time(0)
{
  BRNElement::init();
}

BRN2DNSClient::~BRN2DNSClient()
{
}

void 
BRN2DNSClient::cleanup(CleanupStage stage)
{
  if (CLEANUP_ROUTER_INITIALIZED != stage)
    return; 

  BRN_DEBUG("BRN2DNSClient: end");
}

int
BRN2DNSClient::configure(Vector<String> &conf, ErrorHandler* errh)
{
  BRN_DEBUG("BRN2DNSClient: Configure");

  _active = true;
  if (cp_va_kparse(conf, this, errh,
      "DOMAIN", cpkP, cpString, &_domain,
      "START", cpkP, cpInteger, &_start_time,
      "INTERVAL", cpkP, cpInteger, &_interval,
      "ACTIVE", cpkP, cpBool, &_active,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
        return -1;

  return 0;
}

int
BRN2DNSClient::initialize(ErrorHandler *)
{
  BRN_DEBUG("BRN2DNSClient: Initialize");

  transid = 1;

  _timer.initialize(this);
  if (_active)
    _timer.schedule_after_msec(_start_time);

  return 0;
}

void
BRN2DNSClient::run_timer(Timer* )
{
  Packet *packet_out;

  _timer.reschedule_after_msec(_interval);

  packet_out = DNSProtocol::new_dns_question( _domain, 1, 1);
  DNSProtocol::set_dns_header(packet_out, transid++, DNS_STANDARD_QUERY, 0x0001, 0,0,0);

  _no_requests++;
  _last_request = Timestamp::now();
  _running_request  = true;

  output(0).push(packet_out);
}


void
BRN2DNSClient::push( int, Packet *packet )
{
  uint16_t s;
  char *name;

  BRN_DEBUG("BRN2DNSClient: Push");

  IPAddress ip = IPAddress(DNSProtocol::get_rddata(packet,&s));

  _ip = ip;
  _running_request = false;
  _sum_request_time += (Timestamp::now() - _last_request).msecval();
  _no_replies++;

  name = DNSProtocol::get_name(packet);
  BRN_INFO("DNS-Reply: Name: %s IP: %s",name, ip.unparse().c_str());
  delete[] name;
  packet->kill();

}

String
BRN2DNSClient::print_stats()
{
  StringAccum sa;

  sa << "<dnsclient requested_domains=\"1\" interval=\"" << _interval << "\" >\n";
  sa << "\t<dns_request domain=\"" << _domain << "\" ip=\"" << _ip << "\" requests=\"" << _no_requests << "\" replies=\"";
  sa << _no_replies << "\" avg_response_time=\"";
  if ( _no_replies != 0 ) {
    sa << (_sum_request_time/_no_replies);
  } else {
    sa << "0";
  }

  sa << "\" />\n";
  sa << "</dnsclient>\n";

  return sa.take_string();
}

enum {
  H_STATS,
  H_ACTIVE
};

static String 
read_param(Element *e, void *thunk)
{
  BRN2DNSClient *td = reinterpret_cast<BRN2DNSClient *>(e);
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->print_stats();
    case H_ACTIVE:
      return String(td->_active) + "\n";
    default:
      return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BRN2DNSClient *f = reinterpret_cast<BRN2DNSClient *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_ACTIVE:
    {
      bool active;
      if (!cp_bool(s, &active)) 
        return errh->error("active parameter must be bool");
      f->_active = active;
      if (f->_active && !f->_timer.scheduled()) {
        f->_timer.schedule_after_msec(f->_start_time);
      }
      else if (!f->_active && f->_timer.scheduled())
        f->_timer.unschedule();
      break;
    }
  }
  return 0;
}

void
BRN2DNSClient::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_param, (void *) H_STATS);

  add_read_handler("active", read_param, (void *) H_ACTIVE);
  add_write_handler("active", write_param, (void *) H_ACTIVE);
}

#include <click/vector.cc>
template class Vector<BRN2DNSClient::DNSClientInfo>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DNSClient)
