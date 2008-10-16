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
 * brniapp.{cc,hh} -- handles the inter-ap protocol within brn
 * M. Kurth
 */
 
#include <click/config.h>
#include "elements/brn/common.hh"

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <elements/wifi/wirelessinfo.hh>
#include "brniappnotifyhandler.hh"
#include "elements/brn/nodeidentity.hh"
#include "elements/brn/wifi/ap/assoclist.hh"
#include "elements/brn/routing/linkstat/brnlinktable.hh"
#include "brniappencap.hh"
#include "brniappstationtracker.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappNotifyHandler::BrnIappNotifyHandler() :
  _debug(BrnLogger::DEFAULT),
  _id(NULL),
  _assoc_list(NULL),
  _encap(NULL),
  _sta_tracker(NULL),
  _notify_ms(RESEND_NOTIFY),
  _num_resend(NUM_RESEND)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappNotifyHandler::~BrnIappNotifyHandler()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappNotifyHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
      /* not required */
      cpKeywords,
      "RESEND_NOTIFY", cpUnsigned, "resend notify (ms)", &_notify_ms,
      "NUM_RESEND", cpInteger, "number to resend", &_num_resend,
      "DEBUG", cpInteger, "Debug", &_debug,
      "ASSOCLIST", cpElement, "AssocList element", &_assoc_list,
      "ENCAP", cpElement, "BrnIappNotifyHandler encap element", &_encap,
      "STATRACK", cpElement, "StationTracker element", &_sta_tracker,
      cpEnd) < 0)
    return -1;

  if (!_assoc_list || !_assoc_list->cast("AssocList")) 
    return errh->error("AssocList not specified");

  if (!_encap || !_encap->cast("BrnIappEncap")) 
    return errh->error("BrnIappEncap not specified");

  if (!_sta_tracker || !_sta_tracker->cast("BrnIappStationTracker")) 
    return errh->error("StationTracker not specified");
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappNotifyHandler::initialize(ErrorHandler *errh)
{
  _id = _assoc_list->get_id();
  if (!_id || !_id->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappNotifyHandler::push(int port, Packet *p)
{
  if (0 == port)
  {
    recv_handover_notify(p);
  }
  else if (1 == port)
  {
    recv_handover_reply(p);
  }
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappNotifyHandler::recv_handover_notify(
  Packet* p)
{
  BRN_CHECK_EXPR_RETURN(p == NULL || p->length() < sizeof(struct click_brn_iapp),
    ("invalid argument"), if (p) p->kill(); return;);

  click_brn_iapp*     pIapp   = (click_brn_iapp*)p->data();
  click_brn_iapp_ho*  pHo     = &pIapp->payload.ho;

  BRN_CHECK_EXPR_RETURN(CLICK_BRN_IAPP_HON != pIapp->type,
    ("got invalid iapp type %d", pIapp->type), if (p) p->kill(); return;);

  EtherAddress client(pHo->addr_sta);
  EtherAddress apOld(pHo->addr_mold);
  EtherAddress apNew(pHo->addr_mnew);
  uint8_t seq_no = pHo->seq_no;

  BRN_DEBUG("received notify from %s to %s (client %s)", 
    apNew.s().c_str(), apOld.s().c_str(), client.s().c_str());

  if (CLICK_BRN_IAPP_PAYLOAD_EMPTY == pIapp->payload_type)
  {
    p->kill();p = NULL;pIapp = NULL;
  }
  else {
    BRN_DEBUG("Sending received handover notify to port 1");
    output(1).push(p); 
  }

  // Send handover reply
  send_handover_reply(client, apNew, apOld, seq_no);

  _sta_tracker->sta_roamed(client, apNew, apOld);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappNotifyHandler::recv_handover_reply(
  Packet* p)
{
  BRN_CHECK_EXPR_RETURN(p == NULL || p->length() < sizeof(struct click_brn_iapp),
    ("invalid argument"), if (p) p->kill(); return;);

  click_brn_iapp*     pIapp      = (click_brn_iapp*)p->data();
  click_brn_iapp_ho*  pHo        = &pIapp->payload.ho;

  BRN_CHECK_EXPR_RETURN(CLICK_BRN_IAPP_HOR != pIapp->type,
    ("got invalid iapp type %d", pIapp->type), if (p) p->kill(); return;);

  EtherAddress client(pHo->addr_sta);
  EtherAddress apOld(pHo->addr_mold);
  EtherAddress apNew(pHo->addr_mnew);
  
  BRN_DEBUG("received reply to %s from %s (client %s)", 
    apNew.s().c_str(), apOld.s().c_str(), client.s().c_str());
  
  if (CLICK_BRN_IAPP_PAYLOAD_EMPTY == pIapp->payload_type)
  {
    p->kill();p = NULL;pIapp = NULL;
  }
  else {
    BRN_DEBUG("Sending received handover reply to port 1");
    output(1).push(p); 
  }
  
  NotifyTimer* t = _notifytimer.findp(client);
  // unschedule timer
  if (t != NULL) {
    BRN_INFO("Found notify timer");
    
    // cleanup timer
    cleanup_timer_for_client(client);
  }
  else {
    BRN_WARN("Received reply, but no timer. Timing problem. Maybe reply just received %u late.", _notify_ms);
  }
  
    
  // If the final destination is reached, clean the internals and send reply

  // Cancel retransmission timer
  /// @TODO implement
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappNotifyHandler::send_handover_notify(
  EtherAddress  client, 
  EtherAddress  apNew, 
  EtherAddress  apOld,
  uint8_t       seq_no) 
{
  // Sanity check
  BRN_CHECK_EXPR(!_id->isIdentical(&apNew),
    ("send handover notify with new mesh node %s", apNew.s().c_str()));
  
  BRN_DEBUG("send notify from %s to %s (client %s)", 
    apNew.s().c_str(), apOld.s().c_str(), client.s().c_str());

  // push out
  Packet* p = _encap->create_handover_notify(client, apNew, apOld, seq_no);
  click_ether*        pEther = (click_ether*)p->data();
  click_brn*          pBrn   = (click_brn*)(pEther+1);
  click_brn_iapp*     pIapp  = (click_brn_iapp*)(pBrn+1);
  pIapp->payload_type = CLICK_BRN_IAPP_PAYLOAD_EMPTY;
  
  
  // starting timer
  BRN_DEBUG("Starting timer for client %s", client.s().c_str());
  Timer *t = new Timer(this);
  t->initialize(this);
  
  NotifyTimer timer = NotifyTimer(t, p);
  timer.inc_num_notifies();
  assert(_notifytimer.insert(client, timer) == true);
  
  // 
  t->schedule_now();
  
  //BRN_DEBUG("Sending handover notify to port 0");
  //output(0).push(p);
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappNotifyHandler::send_handover_reply(
  EtherAddress client, 
  EtherAddress apNew, 
  EtherAddress apOld,
  uint8_t      seq_no ) 
{
  // Sanity check
  BRN_CHECK_EXPR(!_id->isIdentical(&apOld),
    ("send handover notify with old mesh node %s", apOld.s().c_str()));
  
  // push out
  Packet* p = _encap->create_handover_reply(client, apNew, apOld, seq_no);
  
  BRN_DEBUG("Sending handover reply to port 0");
  output(0).push(p);
}

void
BrnIappNotifyHandler::run_timer(Timer *t) {
  assert(_notifytimer.size() > 0);
    
  for (NotifyTimersIter i = _notifytimer.begin(); i.live(); i++) {
    NotifyTimer* timer = &i.value();
    if (timer->get_timer() == t) {
      // found timer
      Packet *p = timer->get_packet();
      
      int scheduled = timer->get_num_notifies();
      if (scheduled < _num_resend) {
        BRN_INFO("Found timer. Was scheduled %u times.", scheduled);
        BRN_INFO("Resending every %ums and alltogether %u times.", _notify_ms, _num_resend);
        timer->inc_num_notifies();
        // make a copy of the packet, because it may get killed and we haven't received the reply yet
        Packet *q = p->clone();
        output(0).push(p);
        timer->set_packet(q);
        t->reschedule_after_msec(_notify_ms);          
      }
      else { 
        BRN_WARN("Send %u notifies, but received no reply. Deleting timer.", scheduled);
        cleanup_timer_for_client(i.key());
      }
    }  
  }
}

////////////////////////////////////////////////////////////////////////////////
  
enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappNotifyHandler *td = (BrnIappNotifyHandler *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BrnIappNotifyHandler *f = (BrnIappNotifyHandler *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
BrnIappNotifyHandler::cleanup_timer_for_client(EtherAddress client) {
  NotifyTimer timer = _notifytimer.find(client);
  assert(_notifytimer.remove(client) == true);
  timer.get_packet()->kill();
  delete timer.get_timer();
}


////////////////////////////////////////////////////////////////////////////////

void 
BrnIappNotifyHandler::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

EXPORT_ELEMENT(BrnIappNotifyHandler)
ELEMENT_REQUIRES(brn_common)
#include <click/bighashmap.cc>
CLICK_ENDDECLS

////////////////////////////////////////////////////////////////////////////////