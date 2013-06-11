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
#if CLICK_NS
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS 16
#else
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS 8
#endif

#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE (1 << DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS)
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK (DEFAULT_MAX_BCAST_ID_QUEUE_SIZE - 1)

#define DEFAULT_MAX_BCAST_ID_TIMEOUT    10000
#define FLOODING_LAST_NODE_LIST_DFLT_SIZE 16

     public:
      EtherAddress  _src;

      uint32_t _bcast_id_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _bcast_fwd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];      //no fwd paket
      uint8_t _bcast_fwd_done_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //no fwd done paket (fwd-fwd_done=no_queued_packets
      uint8_t _bcast_fwd_succ_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //no packet recv by at least on node
      uint8_t _bcast_snd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];      //no transmission (incl. retires for unicast)
      uint8_t _bcast_flags_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];    //flags

#define FLOODING_FLAGS_ME_SRC 1

      Timestamp _last_id_time;

      //stats for last node of one packet
      struct flooding_last_node {
        uint8_t etheraddr[6];
        uint8_t received_cnt;
        uint8_t flags;
#define FLOODING_LAST_NODE_FLAGS_FORWARDED  1
      };

      struct flooding_last_node *_last_node_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _last_node_list_size[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _last_node_list_maxsize[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];

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

      ~BroadcastNode()
      {}

      void init() {
        _last_id_time = Timestamp::now();
        reset_queue();
	
	memset(_last_node_list,0,sizeof(_last_node_list));
        memset(_last_node_list_maxsize,0,sizeof(_last_node_list_maxsize));
      }

      void reset_queue() {
        memset(_bcast_id_list, 0, sizeof(_bcast_id_list));
        memset(_bcast_fwd_list, 0, sizeof(_bcast_fwd_list));
        memset(_bcast_fwd_done_list, 0, sizeof(_bcast_fwd_done_list));
        memset(_bcast_fwd_succ_list, 0, sizeof(_bcast_fwd_succ_list));
        memset(_bcast_snd_list, 0, sizeof(_bcast_snd_list));
        memset(_bcast_flags_list, 0, sizeof(_bcast_flags_list));

	memset(_last_node_list_size,0,sizeof(_last_node_list_size));  //all entries are invalid
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

      inline void add_id(uint32_t id, Timestamp now, bool me_src) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        if (_bcast_id_list[index] != id) {
          _bcast_id_list[index] = id;
          _bcast_fwd_list[index] = 0;
          _bcast_fwd_done_list[index] = 0;
          _bcast_fwd_succ_list[index] = 0;
	  _bcast_snd_list[index] = 0;
	  _bcast_flags_list[index] = (me_src)?1:0;
	  
	  _last_node_list_size[index] = 0;
        }
        _last_id_time = now;
      }

      inline bool is_outdated(Timestamp now) {
        return ((now-_last_id_time).msecval() > DEFAULT_MAX_BCAST_ID_TIMEOUT);
      }

      inline bool me_src(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) 
	  return ((_bcast_flags_list[index]&FLOODING_FLAGS_ME_SRC) == FLOODING_FLAGS_ME_SRC);
	return false;
      }

      inline void forward_attempt(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id)
	  _bcast_fwd_list[index]++;
      }

      inline void forward_done(uint32_t id, bool success) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) {
	  _bcast_fwd_done_list[index]++;
	  if (success) _bcast_fwd_succ_list[index]++;
	}
      }
      
      inline int forward_done_cnt(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) return _bcast_fwd_done_list[index];
        return -1;
      }
      
      inline uint32_t forward_attempts(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) return _bcast_fwd_list[index];
        return 0;
      }

      inline uint32_t unfinished_forward_attempts(uint32_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) return _bcast_fwd_list[index]-_bcast_fwd_done_list[index];
        return 0;
      }

      inline void sent(uint32_t id, uint32_t no_transmissions) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) _bcast_snd_list[index] += no_transmissions;
      }

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
	fln[fln_i].received_cnt = fln[fln_i].flags = 0;
        
	if ( forwarded ) fln[fln_i].flags |= FLOODING_LAST_NODE_FLAGS_FORWARDED;
        _last_node_list_size[index]++;
      }

      inline struct flooding_last_node* get_last_node(uint32_t id, EtherAddress *last) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        if ((_last_node_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;
	
	struct flooding_last_node *fln = _last_node_list[index];
	int fln_i = _last_node_list_size[index];

        //search for node
        for ( int i = 0; i < fln_i; i++ )
          if ( memcmp(last->data(), fln[i].etheraddr, 6) == 0 ) return &fln[i];

	return NULL;
      }
      
      inline struct flooding_last_node* get_last_nodes(uint32_t id, uint32_t *size) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

	*size = 0;
        if ((_last_node_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;
	*size = (uint32_t)( _last_node_list_size[index]);
        return _last_node_list[index];
      }
      
      inline void add_recv_last_node(uint32_t id, EtherAddress *last) {
        struct flooding_last_node *fln = get_last_node(id, last);
	if ( fln != NULL ) fln->received_cnt++;
      }
    };

  //
  //methods
  //
  Flooding();
  ~Flooding();

  const char *class_name() const  { return "Flooding"; }
  const char *processing() const  { return PUSH; }

/**
 * 0: broadcast
 * 1: brn
 * 2: txfeedback failure
 * 3: txfeedback success
 * 4: passive overhear
 * 5: low layer reject
 **/

  const char *port_count() const  { return "2-6/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void add_id(EtherAddress *src, uint32_t id, Timestamp *now, bool me_src = false);
  void inc_received(EtherAddress *src, uint32_t id, EtherAddress *last_node);
  bool have_id(EtherAddress *src, uint32_t id, Timestamp *now, uint32_t *fwd_attempts);
  void forward_done(EtherAddress *src, uint32_t id, bool success, bool new_node = false);
  void forward_attempt(EtherAddress *src, uint32_t id);  
  uint32_t unfinished_forward_attempts(EtherAddress *src, uint32_t id);  
  void sent(EtherAddress *src, uint32_t id, uint32_t no_transmission);
  void add_last_node(EtherAddress *src, uint32_t id, EtherAddress *last_node, bool forwarded);
  bool me_src(EtherAddress *src, uint32_t id);

  int retransmit_broadcast(Packet *p, EtherAddress *src, uint16_t bcast_id);

  String stats();
  String table();
  void reset();

  struct Flooding::BroadcastNode::flooding_last_node* get_last_nodes(EtherAddress *src, uint32_t id, uint32_t *size);
  struct Flooding::BroadcastNode::flooding_last_node* get_last_node(EtherAddress *src, uint32_t id, EtherAddress *last);
  
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

  typedef HashMap<EtherAddress, uint32_t> RecvCntMap;
  typedef RecvCntMap::const_iterator RecvCntMapIter;
  RecvCntMap _recv_cnt;

 public:

  uint32_t _flooding_src;
  uint32_t _flooding_rx;
  uint32_t _flooding_sent;
  uint32_t _flooding_fwd;
  uint32_t _flooding_passive;
  uint32_t _flooding_last_node_due_to_passive;
  uint32_t _flooding_last_node_due_to_ack;
  uint32_t _flooding_lower_layer_reject;
    
  uint32_t _flooding_src_new_id;
  uint32_t _flooding_rx_new_id;
  uint32_t _flooding_fwd_new_id;
  
};

CLICK_ENDDECLS
#endif
