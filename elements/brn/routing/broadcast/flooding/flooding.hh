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
#include "floodingpassiveack.hh"


CLICK_DECLS

/*
 * !!! BROADCAST ID -> 0 is never used !!! Don't use 0 as bcastid !!
 */

struct click_brn_bcast {
  uint16_t      bcast_id;
  uint8_t       flags;
  uint8_t       extra_data_size;
} CLICK_SIZE_PACKED_ATTRIBUTE ;

#define FLOODING_EXTRA_STATS

/*
 * =c
 * Flooding()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNFlooding-Packets
 * Input 2  : TXFeedback failure
 * Input 3  : TXFeedback success
 * Input 4  : Passive (Foreign)
 * 
 * Output 0 : Packets to local
 * Output 1 : BRNBroadcastRouting-Packets
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
#ifdef FLOODING_EXTRA_STATS
#define FLOODING_LAST_NODE_LIST_DFLT_SIZE 16
#endif

#define FLOODING_FWD_TRY  1
#define FLOODING_FWD_DONE 2
#define FLOODING_FWD_DONE_SHIFT 1

     public:
      EtherAddress  _src;

      uint32_t _bcast_id_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _bcast_fwd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];

      Timestamp _last_id_time;

#ifdef FLOODING_EXTRA_STATS
      struct flooding_last_node {
        uint8_t etheraddr[6];
        uint8_t forwarded;
      };

      struct flooding_last_node *_last_node_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _last_node_list_size[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _last_node_list_maxsize[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
#endif

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
#ifdef FLOODING_EXTRA_STATS
        memset(_last_node_list,0,sizeof(_last_node_list));
        memset(_last_node_list_size,0,sizeof(_last_node_list_size));
        memset(_last_node_list_maxsize,0,sizeof(_last_node_list_maxsize));
#endif
      }

      void reset_queue() {
        memset(_bcast_id_list, 0, sizeof(_bcast_id_list));
        memset(_bcast_fwd_list, 0, sizeof(_bcast_fwd_list));
#ifdef FLOODING_EXTRA_STATS
        memset(_last_node_list_size,0,sizeof(_last_node_list_size));
#endif
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
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        if (_bcast_id_list[index] != id) {
          _bcast_id_list[index] = id;
          _bcast_fwd_list[index] = 0;
#ifdef FLOODING_EXTRA_STATS
          _last_node_list_size[index] = 0;
#endif
        }
        _last_id_time = now;
      }

      inline bool is_outdated(Timestamp now) {
        return ((now-_last_id_time).msecval() > DEFAULT_MAX_BCAST_ID_TIMEOUT);
      }

      inline void forward_try(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) _bcast_fwd_list[index] |= FLOODING_FWD_TRY;
      }

      inline void forward_done(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) _bcast_fwd_list[index] |= FLOODING_FWD_DONE;
      }

      inline uint32_t forward_attempts(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) {
          if ((_bcast_fwd_list[index]&FLOODING_FWD_DONE) == 0)
            return _bcast_fwd_list[index]&FLOODING_FWD_TRY;
          else
            return (_bcast_fwd_list[index]&FLOODING_FWD_DONE) >> FLOODING_FWD_DONE_SHIFT;
        }
        return 0;
      }

#ifdef FLOODING_EXTRA_STATS
      inline void add_last_node(uint32_t id, EtherAddress *node, bool forwarded) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        if (_last_node_list[index] == NULL) {
          _last_node_list[index] = new struct flooding_last_node[FLOODING_LAST_NODE_LIST_DFLT_SIZE];
          _last_node_list_size[index] = 0;
          _last_node_list_maxsize[index] = FLOODING_LAST_NODE_LIST_DFLT_SIZE;
        } else {
          if ( _last_node_list_size[index] == _last_node_list_maxsize[index] ) {
            _last_node_list_maxsize[index] *= 2; //double
            struct flooding_last_node *new_list = new struct flooding_last_node[_last_node_list_maxsize[index]]; //new list
            memcpy(new_list,_last_node_list[index], _last_node_list_size[index] * sizeof(struct flooding_last_node)); //copy
            delete[] _last_node_list[index]; //delete old
            _last_node_list[index] = new_list; //set new
          }
        }

        struct flooding_last_node *fln = _last_node_list[index];
        int fln_i = _last_node_list_size[index];

        //search for node
        for ( int i = 0; i < fln_i; i++ )
          if ( memcmp(node->data(), fln[i].etheraddr, 6) == 0 ) return;

        memcpy(fln[fln_i].etheraddr, node->data(),6);
        fln[fln_i].forwarded = forwarded?1:0;
        _last_node_list_size[index]++;
      }
#endif
    };

  //
  //methods
  //
  Flooding();
  ~Flooding();

  const char *class_name() const  { return "Flooding"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "2-5/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void add_id(EtherAddress *src, uint32_t id, Timestamp *now);
  bool have_id(EtherAddress *src, uint32_t id, Timestamp *now, uint32_t *fwd_attempts);
  void forward_done(EtherAddress *src, uint32_t id);
  void forward_try(EtherAddress *src, uint32_t id);
#ifdef FLOODING_EXTRA_STATS
  void add_last_node(EtherAddress *src, uint32_t id, EtherAddress *last_node, bool forwarded);
#endif

  int retransmit_broadcast(Packet *p, EtherAddress *src, EtherAddress *fwd, uint16_t bcast_id);

  String stats();
  String table();
  void reset();

 private:
  //
  //member
  //
  BRN2NodeIdentity *_me;

  FloodingPolicy *_flooding_policy;
  FloodingPassiveAck *_flooding_passiveack;

  uint16_t _bcast_id;

  typedef HashMap<EtherAddress, BroadcastNode*> BcastNodeMap;
  typedef BcastNodeMap::const_iterator BcastNodeMapIter;

  BcastNodeMap _bcast_map;

 public:

  uint32_t _flooding_src;
  uint32_t _flooding_rx;
  uint32_t _flooding_sent;
  uint32_t _flooding_fwd;
  uint32_t _flooding_passive;
  
};

CLICK_ENDDECLS
#endif
