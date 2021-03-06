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

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/timer.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/routing/dart/dart_functions.hh"

#include "dart_protocol.hh"
#include "dart_routequerier.hh"
#include "dart_idcache.hh"

CLICK_DECLS

/* constructor initalizes timer, ... */
DartRouteQuerier::DartRouteQuerier() :
    _sendbuffer_timer(static_sendbuffer_timer_hook,(void*)this),
    _me(NULL),
    _dht_storage(NULL),_idcache(NULL),_drt(NULL),
    _debug(BrnLogger::DEFAULT)
{
}

/* destructor processes some cleanup */
DartRouteQuerier::~DartRouteQuerier()
{
  uninitialize();
}

/* called by click at configuration time */
int
DartRouteQuerier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "DHTSTORAGE", cpkP+cpkM, cpElement, &_dht_storage,
      "DARTIDCACHE", cpkP+cpkM, cpElement, &_idcache,
      "DRT", cpkP+cpkM, cpElement, &_drt,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity"))
    return errh->error("NodeIdentity not specified");

  return 0;
}

/* initializes error handler */
int
DartRouteQuerier::initialize(ErrorHandler */*errh*/)
{
  // expire packets in the sendbuffer
  _sendbuffer_timer.initialize(this);

  return 0;
}

/* uninitializes error handler */
void
DartRouteQuerier::uninitialize()
{
  _sendbuffer_timer.unschedule();
}


/*************************************************************************************************/
/******************************* D H T - C A L L B A C K *****************************************/
/*************************************************************************************************/

void
DartRouteQuerier::callback_func(void *e, DHTOperation *op)
{
  DartRouteQuerier *s = reinterpret_cast<DartRouteQuerier *>(e);
  s->callback(op);
}

void
DartRouteQuerier::callback(DHTOperation *op)
{
  BRN_DEBUG("callback %s: Status %d",class_name(),op->header.operation);
  struct dht_nodeid_entry id_entry;

  memcpy((uint8_t*)&id_entry, op->value, op->header.valuelen);
  EtherAddress ea = EtherAddress(op->key);

  if ( op->is_reply() )
  {
    if ( op->header.status == DHT_STATUS_OK ) {
      BRN_DEBUG("READ ID: OK.");
      BRN_DEBUG("EtherAddress %s has id %s",ea.unparse().c_str(),DartFunctions::print_id(id_entry._nodeid, ntohl(id_entry._id_length)).c_str());
      DartIDCache::IDCacheEntry *entry = _idcache->addEntry(&ea, id_entry._nodeid, ntohl(id_entry._id_length));
      send_packets(&ea, entry);
    } else {
      if ( op->header.status == DHT_STATUS_TIMEOUT ) {
        BRN_DEBUG("READ ID: Timeout. Try again !");
      } else {
        BRN_DEBUG("READ ID: NOT FOUND.");
      }
    }
  }

  delete op;
}

/*
 * This method queries the link table for a route to the given destination.
 * If no route is available the packet is buffered a route request is generated.
 */
void
DartRouteQuerier::push(int, Packet *p_in)
{
  BRN_DEBUG("New packet with TTL: %d",(uint32_t)BRNPacketAnno::ttl_anno(p_in));

  DartIDCache::IDCacheEntry *entry;

  if ( p_in->length() < sizeof(click_ether) ) {
    BRN_DEBUG(" * packet too small to be etherframe; drop packet.");
    p_in->kill();
    return;
  }

  const click_ether *ether = reinterpret_cast<const click_ether *>(p_in->data());  //better to use this, since ether_header is not always set.it also can be overwriten

  EtherAddress dst_addr(ether->ether_dhost);

  if (!dst_addr) {
    BRN_ERROR(" ethernet anno header is null; kill packet.");
    p_in->kill();
    return;
  }

  BRN_DEBUG("Request for %s", dst_addr.unparse().c_str());

  DHTnode *n = _drt->get_node(&dst_addr);
  entry = _idcache->getEntry(&dst_addr);

  if ( entry == NULL ) {
    if ( n != NULL ) {
      BRN_DEBUG("Got node from routingtable !");
      entry = _idcache->addEntry(&dst_addr, n->_md5_digest, n->_digest_length);
    }
  } else {
    if ( n != NULL ) {
      entry->update(n);
    }
  }

  BRN_DEBUG("Dst: %s", dst_addr.unparse().c_str());
  //BRN_DEBUG("Routing-Table:\n %s",_drt->routing_info().c_str());

  if ( entry == NULL ) {
    _packet_buffer.addPacket_ms(p_in, BRN_DART_SENDBUFFER_TIMER_INTERVAL - 1000, 0);   //push in packet queue
    start_issuing_request(&dst_addr);            //start request for ID
  } else {
    BRN_DEBUG("Route querier: Has ID in Cache");
    BRN_DEBUG("Dst: %s (%s)", dst_addr.unparse().c_str(),
              DartFunctions::print_id(entry->_nodeid, entry->_id_length).c_str());
    WritablePacket *dart_p = DartProtocol::add_route_header(entry->_nodeid, entry->_id_length, _drt->_me->_md5_digest, _drt->_me->_digest_length,_drt->_ident, p_in);

    output(0).push(dart_p);
  }

  return;
}

void
DartRouteQuerier::send_packets(EtherAddress *dst, DartIDCache::IDCacheEntry *entry )
{
  for ( int i = _packet_buffer.size() - 1; i >= 0; i-- ) {
    PacketSendBuffer::BufferedPacket *buffp = _packet_buffer.get(i);

    const click_ether *ether = reinterpret_cast<const click_ether *>(buffp->_p->data());

    if ( memcmp(ether->ether_dhost, dst->data(),6) == 0 ) {
      WritablePacket *dart_p = DartProtocol::add_route_header(entry->_nodeid, entry->_id_length, _drt->_me->_md5_digest, _drt->_me->_digest_length,_drt->_ident, buffp->_p);

      _packet_buffer.del(i);

      BRN_DEBUG("Send packet with TTL: %d",(uint32_t)BRNPacketAnno::ttl_anno(dart_p));

      output(0).push(dart_p);
    }
  }

  del_requests_for_ea(dst);
}

//-----------------------------------------------------------------------------
// Timer-driven events
//-----------------------------------------------------------------------------

DartRouteQuerier::RequestAddress *
DartRouteQuerier::requests_for_ea(EtherAddress *ea )
{
  for ( int i = _request_list.size() - 1; i >= 0; i-- ) {
    DartRouteQuerier::RequestAddress *rea = _request_list[i];
    if ( memcmp(rea->_ea.data(), ea->data(),6) == 0 ) return rea;
  }

  return NULL;
}

void
DartRouteQuerier::del_requests_for_ea(EtherAddress *ea)
{
  del_requests_for_ea(ea->data());
}

void
DartRouteQuerier::del_requests_for_ea(const uint8_t *ea)
{
  for ( int i = _request_list.size() - 1; i >= 0; i-- ) {
    DartRouteQuerier::RequestAddress *rea = _request_list[i];
    if ( memcmp(rea->_ea.data(), ea, 6) == 0 ) {
      delete rea;
      _request_list.erase(_request_list.begin() + i);
      return;
    }
  }
}

/*
 * start issuing requests for a host.
 */
void
DartRouteQuerier::start_issuing_request(EtherAddress *dst)
{
  DartRouteQuerier::RequestAddress *rea = requests_for_ea(dst);

  if ( rea == NULL ) _request_list.push_back(new RequestAddress(dst));
  else return;

  DHTOperation *dhtop;
  int result;

  dhtop = new DHTOperation();

  BRN_DEBUG("Search EtherAddress: %s",dst->unparse().c_str());

  dhtop->read(dst->data(), 6);
  dhtop->max_retries = 1;

  result = _dht_storage->dht_request(dhtop, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_DEBUG("Got direct-reply (local)");
    callback_func((void*)this, dhtop);
  } else {
    _sendbuffer_timer.reschedule_after_msec(BRN_DART_SENDBUFFER_TIMER_INTERVAL);
  }
}

/* functions to manage the packet_timeouts*/
void
DartRouteQuerier::static_sendbuffer_timer_hook(Timer *, void *v)
{
  DartRouteQuerier *rt = reinterpret_cast<DartRouteQuerier*>(v);
  rt->sendbuffer_timer_hook();
}

void
DartRouteQuerier::sendbuffer_timer_hook()
{
  BRN_DEBUG("Packet Timeout. Check for packets.");

  for ( int i = _packet_buffer.size() - 1; i >= 0; i-- ) {
    PacketSendBuffer::BufferedPacket *buffp = _packet_buffer.get(i);
    if ( buffp->timeout() ) {
      BRN_DEBUG("Kill packet");

      const click_ether *ether = reinterpret_cast<const click_ether *>(buffp->_p->data());
      del_requests_for_ea(ether->ether_dhost);

      buffp->_p->kill();
      _packet_buffer.del(i);
      delete buffp;
    }
  }
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {H_DEBUG, H_FIXED_ROUTE, H_FIXED_ROUTE_CLEAR, H_FLUSH_SB};

static String
read_handler(Element *e, void * vparam)
{
  DartRouteQuerier *rq = reinterpret_cast<DartRouteQuerier *>(e);

  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      return String(rq->_debug) + "\n";
    }
  }
  return String("n/a\n");
}

static int 
write_handler(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  DartRouteQuerier *rq = reinterpret_cast<DartRouteQuerier *>(e);
  String s = cp_uncomment(in_s);

  switch ((intptr_t)vparam) {
    case H_DEBUG: {
      int debug;
      if (!cp_integer(s, &debug))
        return errh->error("debug parameter must be an integer value between 0 and 4");
      rq->_debug = debug;
      break;
    }
  }
  return 0;
}

void
DartRouteQuerier::add_handlers()
{
  add_read_handler("debug", read_handler, (void*) H_DEBUG);

  add_write_handler("debug", write_handler, (void*) H_DEBUG);
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(PacketSendBuffer)
EXPORT_ELEMENT(DartRouteQuerier)


