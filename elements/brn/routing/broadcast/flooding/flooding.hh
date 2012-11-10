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

#ifndef FLOODINGELEMENT_HH
#define FLOODINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timestamp.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/timestamp.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "floodingpolicy/floodingpolicy.hh"

CLICK_DECLS

struct click_brn_bcast {
  uint16_t      bcast_id;
  uint8_t       flags;
  uint8_t       extra_data_size;
} CLICK_SIZE_PACKED_ATTRIBUTE ;


/*
 * =c
 * Flooding()
 * =s
 *
 * =d
 */

class Flooding : public BRNElement {

 public:

   class BroadcastNode
   {
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS 8
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE (1 << DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS)
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK (DEFAULT_MAX_BCAST_ID_QUEUE_SIZE - 1)

#define DEFAULT_MAX_BCAST_ID_TIMEOUT    10000

     public:
      EtherAddress  _src;

      uint32_t _bcast_id_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //TODO: use comination of hashmap and vector

      Timestamp _last_id_time;

      BroadcastNode()
      {
        _src = EtherAddress::make_broadcast();
        init();
      }

      BroadcastNode( EtherAddress *src )
      {
        _src = *src;
        init();
      }

      BroadcastNode( EtherAddress *src, uint32_t id )
      {
        _src = *src;
        init();
        add_id(id, Timestamp::now());
      }

      ~BroadcastNode()
      {}

      void init() {
        _last_id_time = Timestamp::now();
        reset_queue();
      }

      void reset_queue() {
        memset(_bcast_id_list, 0, sizeof(_bcast_id_list));
      }

      inline bool have_id(uint32_t id) {
        return (_bcast_id_list[id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK] == id );
      }

      inline bool have_id(uint32_t id, Timestamp now) {
        if ( is_outdated(now) ) {
          reset_queue();
          return false;
        }

        return have_id(id);
      }

      inline void add_id(uint32_t id, Timestamp now) {
        _bcast_id_list[id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK] = id;
        _last_id_time = now;
      }

      inline bool is_outdated(Timestamp now) {
        return ((now-_last_id_time).msecval() > DEFAULT_MAX_BCAST_ID_TIMEOUT);
      }
    };

  //
  //methods
  //
  Flooding();
  ~Flooding();

  const char *class_name() const  { return "Flooding"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "3/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void add_id(EtherAddress *src, uint32_t id, Timestamp *now);
  bool have_id(EtherAddress *src, uint32_t id, Timestamp *now);

  String stats();
  String table();
  void reset();

 private:
  //
  //member
  //
  BRN2NodeIdentity *_me;

  FloodingPolicy *_flooding_policy;

  uint16_t _bcast_id;

  typedef HashMap<EtherAddress, BroadcastNode*> BcastNodeMap;
  typedef BcastNodeMap::const_iterator BcastNodeMapIter;

  BcastNodeMap _bcast_map;

 public:

  uint32_t _flooding_src;
  uint32_t _flooding_fwd;
};

CLICK_ENDDECLS
#endif
