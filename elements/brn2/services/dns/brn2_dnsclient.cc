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
#include "elements/brn/common.hh"
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>


#include "brn2_dnsclient.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "dnsprotocol.hh"

CLICK_DECLS

BRN2DNSClient::BRN2DNSClient()
  : _debug(BrnLogger::DEFAULT), _timer(this)
{
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
  output(0).push(packet_out);
}


void
BRN2DNSClient::push( int, Packet *packet )
{
  BRN_DEBUG("BRN2DNSClient: Push");

  packet->kill();

}


enum {
  H_DEBUG,
  H_ACTIVE
};

static String 
read_param(Element *e, void *thunk)
{
  BRN2DNSClient *td = (BRN2DNSClient *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
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
  BRN2DNSClient *f = (BRN2DNSClient *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: 
    {    //debug
      int debug;
      if (!cp_integer(s, &debug)) 
        return errh->error("debug parameter must be int");
      f->_debug = debug;
      break;
    }
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
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("active", read_param, (void *) H_ACTIVE);
  add_write_handler("active", write_param, (void *) H_ACTIVE);
}

#include <click/vector.cc>
template class Vector<BRN2DNSClient::DNSClientInfo>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DNSClient)
