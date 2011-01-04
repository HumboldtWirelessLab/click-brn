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

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "floodingpolicy/floodingpolicy.hh"

CLICK_DECLS

struct click_brn_bcast {
  uint16_t      bcast_id;
};


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
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE 50
#define DEFAULT_MAX_BCAST_ID_TIMEOUT    10000

     public:
      EtherAddress  _src;

      int32_t _bcast_id_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //TODO: use comination of hashmap and vector
      uint32_t _next_index;

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

      BroadcastNode( EtherAddress *src, int32_t id )
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
        for( int i = 0; i < DEFAULT_MAX_BCAST_ID_QUEUE_SIZE; i++ ) _bcast_id_list[i] = -1;
        _next_index = 0;
      }

      bool have_id(int32_t id) {
        for( int i = 0; i < DEFAULT_MAX_BCAST_ID_QUEUE_SIZE; i++ ) {
          if ( _bcast_id_list[i] == id ) return true;
          if ( _bcast_id_list[i] == -1 ) return false;
        }

        return false;
      }

      inline bool have_id(int32_t id, Timestamp now) {
        if ( is_outdated(now) ) {
          reset_queue();
          return false;
        }

        return have_id(id);
      }

      inline void add_id(int32_t id, Timestamp now) {
        _bcast_id_list[_next_index] = id;
        _next_index = (_next_index + 1) % DEFAULT_MAX_BCAST_ID_QUEUE_SIZE;
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
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void add_id(EtherAddress *src, int32_t id, Timestamp *now);
  bool have_id(EtherAddress *src, int32_t id, Timestamp *now);

  String stats();
  void reset();

 private:
  //
  //member
  //
  FloodingPolicy *_flooding_policy;

  uint16_t bcast_id;

  typedef HashMap<EtherAddress, BroadcastNode> BcastNodeMap;
  typedef BcastNodeMap::const_iterator BcastNodeMapIter;

  BcastNodeMap bcast_map;

 public:

  int _flooding_src;
  int _flooding_fwd;
};

CLICK_ENDDECLS
#endif
