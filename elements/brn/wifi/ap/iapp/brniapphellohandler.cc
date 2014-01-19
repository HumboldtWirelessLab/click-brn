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
 * brniapphellohandler.{cc,hh} -- handles the inter-ap protocol within brn
 * M. Kurth
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brniapphellohandler.hh"
#include "brniappencap.hh"
#include "brniapprotocol.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

int BrnIappHelloHandler::_hello_trigger_interval_ms = 0;

////////////////////////////////////////////////////////////////////////////////

BrnIappHelloHandler::BrnIappHelloHandler() :
  _debug(BrnLogger::DEFAULT),
  _optimize(true),
  _timer(static_timer_trigger, this),
  _timer_hello(static_timer_hello_trigger, this),
  _id(NULL),
  _assoc_list(NULL),
  _link_table(NULL),
  _encap(NULL)
{
  BRNElement::init();
}

////////////////////////////////////////////////////////////////////////////////

BrnIappHelloHandler::~BrnIappHelloHandler()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappHelloHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _hello_trigger_interval_ms = 0;

  if (cp_va_kparse(conf, this, errh,
      "STALE", cpkP+cpkM, cpUnsigned, /*"Stale info timeout (ms)",*/ &_hello_trigger_interval_ms,
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"AssocList element",*/ &_assoc_list,
      "ENCAP", cpkP+cpkM, cpElement,/* "BrnIapp encap element",*/ &_encap,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_id,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "OPTIMIZE", cpkP, cpBool, /*"Optimize",*/ &_optimize,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      cpEnd) < 0)
    return -1;

  if (!_assoc_list || !_assoc_list->cast("BRN2AssocList")) 
    return errh->error("BRN2AssocList not specified");

  if (!_encap || !_encap->cast("BrnIappEncap")) 
    return errh->error("BrnIappEncap not specified");

  if (!_id || !_id->cast("BRN2NodeIdentity"))
    return errh->error("BRN2NodeIdentity not specified");

  if (!_link_table || !_link_table->cast("Brn2LinkTable"))
    return errh->error("Brn2LinkTable not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappHelloHandler::initialize(ErrorHandler */*errh*/)
{
  click_brn_srandom();
  // if not specified, use half of the link table stale timeout
  if (0 >= _hello_trigger_interval_ms) {
    timeval t;
    _link_table->get_stale_timeout(t);

    _hello_trigger_interval_ms = t.tv_sec  * 1000 + t.tv_usec / 1000;
    _hello_trigger_interval_ms /= 2;
  }

  _timer.initialize(this);
  _timer_hello.initialize(this);

  // Jitter the start for the simulator
  unsigned int _min_jitter  = _hello_trigger_interval_ms/2 /* ms */;
  unsigned int _jitter      = _hello_trigger_interval_ms;

  unsigned int j = (unsigned int) ( _min_jitter +( click_random() % ( _jitter ) ) );
  _timer.schedule_after_msec(j);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappHelloHandler::push(int, Packet *p)
{
  if (!_optimize)
  {
    BRN_DEBUG("optimization turned off, killing packet.");
    if (p)
      p->kill();
    return;
  }

  BRN_CHECK_EXPR_RETURN(p == NULL || p->length() < sizeof(struct click_brn_iapp),
    ("invalid argument"), if (p) p->kill(); return;);

  click_brn_iapp*     pIapp   = (click_brn_iapp*)p->data();
  click_brn_iapp_he*  pHe     = &pIapp->payload.he;

  BRN_CHECK_EXPR_RETURN(CLICK_BRN_IAPP_HEL != pIapp->type,
    ("got invalid iapp type %d", pIapp->type), if (p) p->kill(); return;);

  EtherAddress sta(pHe->addr_sta);
  EtherAddress ap_curr(pHe->addr_ap_curr);
  EtherAddress ap_cand(pHe->addr_ap_cand);
  //uint8_t authoritive = pHe->authoritive;

  p->kill();

  BRN_DEBUG("received hello curr %s cand %s (client %s)", 
    ap_curr.unparse().c_str(), ap_cand.unparse().c_str(), sta.unparse().c_str());

  // Generate iapp hello reply, if it is our client
  if (_id->isIdentical(&ap_curr)
    && !_id->isIdentical(&ap_cand)) 
  {
    // First check if we know the client, otherwise return
    BRN2AssocList::client_state state = _assoc_list->get_state(sta);
    if (BRN2AssocList::NON_EXIST == state
      || BRN2AssocList::SEEN_OTHER == state)
      return;

    send_iapp_hello(sta, ap_cand, _assoc_list->get_ap(sta), false);
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappHelloHandler::send_iapp_hello(
  EtherAddress sta,
  EtherAddress ap_cand, 
  EtherAddress ap_curr,
  bool         to_curr)
{
  if (!_optimize)
  {
    BRN_DEBUG("optimization turned off, not sending hello.");
    return;
  }

  EtherAddress& src(to_curr ? ap_cand : ap_curr);
  EtherAddress& dst(to_curr ? ap_curr : ap_cand);

  BRN_DEBUG("send hello from %s to %s (client %s)", 
    src.unparse().c_str(), dst.unparse().c_str(), sta.unparse().c_str());

  // push out
  Packet* p = _encap->create_iapp_hello(sta, ap_cand, ap_curr, to_curr);
  output(0).push(p);
}
////////////////////////////////////////////////////////////////////////////////

void
BrnIappHelloHandler::schedule_hello(
  EtherAddress sta)
{
  if (!_optimize)
  {
    BRN_DEBUG("optimization turned off, not scheduling hello.");
    return;
  }

  // Do not send immediatly, because otherwise route requests will always collide
  // with hello messages.

  // Add the sta to the parameter list
  _sta_hello_queue.push_back(sta);
  BRN_DEBUG("added sta %s to hello queue", sta.unparse().c_str());

  // if already scheduled, then do not reschedule
  if (_timer_hello.scheduled())
    return;

  // TODO make configurable
  unsigned int min_jitter = 5;
  unsigned int jitter     = 100;
  unsigned int j = (unsigned int ) ( min_jitter +( click_random() % ( jitter ) ) );

  _timer_hello.schedule_after_msec(j);  
  BRN_DEBUG("scheduled hello in %s ms", _timer.expiry().unparse().c_str());
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappHelloHandler::send_hello()
{
  // go through the parameter list and send a hello
  while (0 < _sta_hello_queue.size())
  {
    EtherAddress sta = _sta_hello_queue.front();
    _sta_hello_queue.pop_front();

    BRN2AssocList::ClientInfo* pClient = _assoc_list->get_entry(sta);

    // Could be null in the case the client moved away in the meantime
    if (pClient == NULL) {
        BRN_INFO("canceling hello for client %s, since it is unknown now.",
          sta.unparse().c_str());
        continue;
    }

    // Send the hello after timeout
    send_iapp_hello(  pClient->get_eth(),
                      *(_id->getMasterAddress()),
                      pClient->get_ap(),
                      true);
  }
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappHelloHandler::hello_trigger()
{
  // Go through all clients 
  for (BRN2AssocList::iterator i = _assoc_list->begin(); i.live(); i++)
  {
    BRN2AssocList::ClientInfo& nfo = i.value();
    if (BRN2AssocList::SEEN_BRN != nfo.get_state()
      && BRN2AssocList::ROAMED != nfo.get_state())
      continue;

    // Send the hello after timeout
    send_iapp_hello(  nfo.get_eth(),
                      *_id->getMasterAddress(),
                      nfo.get_ap(),
                      true);
  }
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappHelloHandler::static_timer_trigger(Timer *t, void *v)
{
  BrnIappHelloHandler *as = (BrnIappHelloHandler *)v;
  as->hello_trigger();
  t->schedule_after_msec(_hello_trigger_interval_ms);
}

////////////////////////////////////////////////////////////////////////////////

void
BrnIappHelloHandler::static_timer_hello_trigger(Timer *, void *v)
{
  BrnIappHelloHandler *as = (BrnIappHelloHandler *)v;
  as->send_hello();
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG, H_OPTIMIZE};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappHelloHandler *td = (BrnIappHelloHandler *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  case H_OPTIMIZE:
    return String(td->_optimize) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BrnIappHelloHandler *f = (BrnIappHelloHandler *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  case H_OPTIMIZE:
    {    //debug
      bool optimize;
      if (!cp_bool(s, &optimize)) 
        return errh->error("optimize parameter must be boolean");
      f->_optimize = optimize;
      break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappHelloHandler::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);

  add_read_handler("optimize", read_param, (void *) H_OPTIMIZE);
  add_write_handler("optimize", write_param, (void *) H_OPTIMIZE);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappHelloHandler)

////////////////////////////////////////////////////////////////////////////////
