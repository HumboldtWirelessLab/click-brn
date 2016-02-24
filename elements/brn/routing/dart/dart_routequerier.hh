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

#ifndef DARTROUTEQUERIER_HH
#define DARTROUTEQUERIER_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn/standard/packetsendbuffer.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"
#include "elements/brn/dht/routing/dart/dart_routingtable.hh"


#include "dart_idcache.hh"

#define BRN_DART_SENDBUFFER_TIMER_INTERVAL 3000

CLICK_DECLS

class DartRouteQuerier : public Element
{
 public:

   class RequestAddress {
     public:
       EtherAddress _ea;
       int _no;

       explicit RequestAddress(EtherAddress *ea): _ea(ea->data()), _no(1) {
       }

       void inc() { _no++; }
   };

  //---------------------------------------------------------------------------
  // public methods
  //---------------------------------------------------------------------------

  DartRouteQuerier();
  ~DartRouteQuerier();

  const char *class_name() const { return "DartRouteQuerier"; }
  const char *port_count() const { return "1/2"; }
  const char *processing() const { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void push(int, Packet *);

  static void callback_func(void *e, DHTOperation *op);
  void callback(DHTOperation *op);

  Vector<RequestAddress*> _request_list;

 private:
   RequestAddress *requests_for_ea(EtherAddress *ea );
   void del_requests_for_ea(EtherAddress *ea);
   void del_requests_for_ea(const uint8_t *ea);

   void start_issuing_request(EtherAddress *dst);
   void send_packets(EtherAddress *dst, DartIDCache::IDCacheEntry *entry );
  //---------------------------------------------------------------------------
  // private fields
  //---------------------------------------------------------------------------
  Timer _sendbuffer_timer;
  static void static_sendbuffer_timer_hook(Timer *, void *);
  void sendbuffer_timer_hook();

  BRN2NodeIdentity *_me;
  DHTStorage *_dht_storage;
  DartIDCache *_idcache;
  PacketSendBuffer _packet_buffer;
  DartRoutingTable *_drt;
  //---------------------------------------------------------------------------
  // fields
  //---------------------------------------------------------------------------
 public:
  int _debug;

};

CLICK_ENDDECLS
#endif
