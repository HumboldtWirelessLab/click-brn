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

/**  TODO:
 * forward a known packet again? ask   policy if the packet is known ?
 */
/*
 * !!! BROADCAST ID -> 0 is never used !!! Don't use 0 as bcastid !!
 */

struct click_brn_bcast {
  uint16_t      bcast_id;
  uint8_t       flags;
  uint8_t       extra_data_size;
} CLICK_SIZE_PACKED_ATTRIBUTE ;

#define BCAST_HEADER_FLAGS_FORCE_DST           1 /* src is responsible for target */
#define BCAST_HEADER_FLAGS_REJECT_ON_EMPTY_CS  2 /* reason for reject was empty cs */
#define BCAST_HEADER_FLAGS_REJECT_WITH_ASSIGN  4 /* reason for reject was empty cs with assigned nodes*/

#define BCAST_MAX_EXTRA_DATA_SIZE            255

struct click_brn_bcast_extra_data {
  uint8_t size;
  uint8_t type;
} CLICK_SIZE_PACKED_ATTRIBUTE ;

#define BCAST_EXTRA_DATA_MPR                    1
#define BCAST_EXTRA_DATA_LASTNODE               2

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
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS  8 
#else
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS  8
#endif

#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE (1 << DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_BITS)
#define DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK (DEFAULT_MAX_BCAST_ID_QUEUE_SIZE - 1)

#define DEFAULT_MAX_BCAST_ID_TIMEOUT       20000
#define FLOODING_LAST_NODE_LIST_DFLT_SIZE     16

     public:
      EtherAddress  _src;

      uint16_t _bcast_id_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _bcast_fwd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];      //no fwd paket
      uint8_t _bcast_fwd_done_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //no fwd done paket (fwd-fwd_done=no_queued_packets
      uint8_t _bcast_fwd_succ_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //no packet recv by at least on node
      uint8_t _bcast_snd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];      //no transmission (incl. retries for unicast)
      uint8_t _bcast_flags_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];    //flags
      Timestamp _bcast_time_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];   //time

#define FLOODING_FLAGS_ME_SRC           1

      Timestamp _last_id_time;           //timeout for hole queue TODO: remove since its deprecated

      //stats for last node of one packet
      struct flooding_last_node {
        uint8_t etheraddr[6];
        uint8_t received_cnt;
        uint8_t flags;
#define FLOODING_LAST_NODE_FLAGS_FORWARDED                  1
#define FLOODING_LAST_NODE_FLAGS_RX_ACKED                   2
#define FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY             4
#define FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY     8

#define FLOODING_LAST_NODE_FLAGS_FINISHED_FOR_ME            (FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY | FLOODING_LAST_NODE_FLAGS_RX_ACKED)
#define FLOODING_LAST_NODE_FLAGS_FINISHED_FOR_FOREIGN       (FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY | FLOODING_LAST_NODE_FLAGS_RX_ACKED)

#define FLOODING_LAST_NODE_FLAGS_FINISHED_RESPONSIBILITY   16

#define FLOODING_LAST_NODE_FLAGS_REVOKE_ASSIGN            128

      };

      struct flooding_last_node *_last_node_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _last_node_list_size[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _last_node_list_maxsize[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];

      struct flooding_last_node *_assigned_node_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _assigned_node_list_size[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
      uint8_t _assigned_node_list_maxsize[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];

      bool _fix_target_set;

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
        memset(_assigned_node_list,0,sizeof(_assigned_node_list));
        memset(_assigned_node_list_maxsize,0,sizeof(_assigned_node_list_maxsize));
      }

      void reset_queue() {
        memset(_bcast_id_list, 0, sizeof(_bcast_id_list));
        memset(_bcast_fwd_list, 0, sizeof(_bcast_fwd_list));
        memset(_bcast_fwd_done_list, 0, sizeof(_bcast_fwd_done_list));
        memset(_bcast_fwd_succ_list, 0, sizeof(_bcast_fwd_succ_list));
        memset(_bcast_snd_list, 0, sizeof(_bcast_snd_list));
        memset(_bcast_flags_list, 0, sizeof(_bcast_flags_list));

        memset(_last_node_list_size,0,sizeof(_last_node_list_size));          //all entries are invalid
        memset(_assigned_node_list_size,0,sizeof(_assigned_node_list_size));  //all entries are invalid
        _fix_target_set = false;
      }

      inline bool have_id(uint16_t id) {
        //TODO: test for timeout ? its imortant but slow (it can remove global time (_last_id_time))
        return (_bcast_id_list[id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK] == id );
      }

      inline bool have_id(uint16_t id, Timestamp now) {
        if ( is_outdated(now) ) {
          reset_queue();
          return false;
        }

        return have_id(id);
      }

      inline bool is_outdated(Timestamp now) {
        return ((now-_last_id_time).msecval() > DEFAULT_MAX_BCAST_ID_TIMEOUT);
      }

      /**
       * TODO: timeout for ids
       **/
      void add_id(uint16_t id, Timestamp now, bool me_src) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        if (_bcast_id_list[index] != id) {
          _bcast_id_list[index] = id;
          _bcast_fwd_list[index] = 0;
          _bcast_fwd_done_list[index] = 0;
          _bcast_fwd_succ_list[index] = 0;
          _bcast_snd_list[index] = 0;
          _bcast_flags_list[index] = (me_src)?1:0;
          _bcast_time_list[index] = now;

          _last_node_list_size[index] = 0;
          _assigned_node_list_size[index] = 0;
        }
        _last_id_time = now;
      }

      inline bool me_src(uint16_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) 
          return ((_bcast_flags_list[index]&FLOODING_FLAGS_ME_SRC) == FLOODING_FLAGS_ME_SRC);
        return false;
      }

      inline void forward_attempt(uint16_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id)
        _bcast_fwd_list[index]++;
      }

      inline void forward_done(uint16_t id, bool success) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) {
          _bcast_fwd_done_list[index]++;
          if (success) _bcast_fwd_succ_list[index]++;
        }
      }

      inline int forward_done_cnt(uint16_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) return _bcast_fwd_done_list[index];
        return -1;
      }

      inline uint8_t forward_attempts(uint16_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) return _bcast_fwd_list[index];
        return 0;
      }

      inline uint8_t unfinished_forward_attempts(uint16_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) return _bcast_fwd_list[index]-_bcast_fwd_done_list[index];
        return 0;
      }

      inline void sent(uint16_t id, uint32_t no_transmissions) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id) _bcast_snd_list[index] += no_transmissions;
      }

      /**
       * Last node (known nodes)
       */

      int add_last_node(uint16_t id, EtherAddress *node, bool forwarded, bool rx_acked, bool responsibility, bool foreign_responsibility) {
        assert(forwarded || rx_acked || responsibility || foreign_responsibility );

        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] != id) return -1;

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
        for ( int i = 0; i < fln_i; i++ ) {
          if ( memcmp(node->data(), fln[i].etheraddr, 6) == 0 ) {                //found node

            if ( forwarded ) fln[i].flags |= FLOODING_LAST_NODE_FLAGS_FORWARDED;
            if ( rx_acked ) {
              fln[i].flags |= FLOODING_LAST_NODE_FLAGS_RX_ACKED;                  //is acked ? so mark it
              fln[i].flags &= ~FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY;   //clear foreign_responsibility
              if ( fln[i].flags & FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY )       //i was resp? 
                fln[i].flags |= FLOODING_LAST_NODE_FLAGS_FINISHED_RESPONSIBILITY; //then mark it
              fln[i].flags &= ~FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY;           //clear responsibility
            } else if ( responsibility ) {
              fln[i].flags |= FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY;            //set responsibility
              fln[i].flags &= ~FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY;   //clear foreign_responsibility
            } else if (foreign_responsibility && ((fln[i].flags & FLOODING_LAST_NODE_FLAGS_FINISHED_FOR_FOREIGN) == 0)) {
                fln[i].flags |= FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY;
            }

            assert(fln[i].flags != 0);

            return 0;
          }
        }
        /*
         * Node not found, so add new_list
         */
        memcpy(fln[fln_i].etheraddr, node->data(),6);
        fln[fln_i].received_cnt = fln[fln_i].flags = 0;

        if ( forwarded ) fln[fln_i].flags |= FLOODING_LAST_NODE_FLAGS_FORWARDED;
        if ( rx_acked )  fln[fln_i].flags |= FLOODING_LAST_NODE_FLAGS_RX_ACKED;
        else if ( responsibility ) fln[fln_i].flags |= FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY;
        else if ( foreign_responsibility ) fln[fln_i].flags |= FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY;

        _last_node_list_size[index]++;

        /* revoke assignment since node is new */
        revoke_assigned_node(id, node);

        assert(fln[fln_i].flags != 0 );

        return _last_node_list_size[index];
      }

      struct flooding_last_node* get_last_node(uint16_t id, EtherAddress *last) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        if ((_last_node_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;

        struct flooding_last_node *fln = _last_node_list[index];
        int fln_i = _last_node_list_size[index];

        //search for node
        for ( int i = 0; i < fln_i; i++ )
          if ( memcmp(last->data(), fln[i].etheraddr, 6) == 0 ) return &fln[i];

        return NULL;
      }

      struct flooding_last_node* get_last_nodes(uint16_t id, uint32_t *size) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        *size = 0;
        if ((_last_node_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;
        *size = (uint32_t)( _last_node_list_size[index]);
        return _last_node_list[index];
      }

      inline void add_recv_last_node(uint16_t id, EtherAddress *last) {
        struct flooding_last_node *fln = get_last_node(id, last);
        if ( fln != NULL ) fln->received_cnt++;
      }

      /**
       * ASSIGN: flags wether the packet is transmited to a node A (LastNode) by another node
       */

      int assign_node(uint16_t id, EtherAddress *node) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] != id) return -1;

        if (_assigned_node_list[index] == NULL) {
          _assigned_node_list[index] = new struct flooding_last_node[FLOODING_LAST_NODE_LIST_DFLT_SIZE];
          _assigned_node_list_size[index] = 0;
          _assigned_node_list_maxsize[index] = FLOODING_LAST_NODE_LIST_DFLT_SIZE;
        } else {
          if ( _assigned_node_list_size[index] == _assigned_node_list_maxsize[index] ) {
            _assigned_node_list_maxsize[index] *= 2; //double
            struct flooding_last_node *new_list = new struct flooding_last_node[_assigned_node_list_maxsize[index]]; //new list
            memcpy(new_list,_assigned_node_list[index], _assigned_node_list_size[index] * sizeof(struct flooding_last_node)); //copy
            delete[] _assigned_node_list[index]; //delete old
            _assigned_node_list[index] = new_list; //set new
          }
        }

        struct flooding_last_node *fln = _assigned_node_list[index];
        int fln_i = _assigned_node_list_size[index];

        //search for node
        for ( int i = 0; i < fln_i; i++ )
          if ( memcmp(node->data(), fln[i].etheraddr, 6) == 0 ) return 0;

        memcpy(fln[fln_i].etheraddr, node->data(),6);
        fln[fln_i].received_cnt = fln[fln_i].flags = 0;

        _assigned_node_list_size[index]++;

        return _assigned_node_list_size[index];
      }

      struct flooding_last_node* get_assigned_nodes(uint16_t id, uint32_t *size) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

        *size = 0;
        if ((_assigned_node_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;
        *size = (uint32_t)( _assigned_node_list_size[index]);
        return _assigned_node_list[index];
      }

      void clear_assigned_nodes(uint16_t id) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        if (_bcast_id_list[index] == id)
          _assigned_node_list_size[index] = 0;
      }

      void revoke_assigned_node(uint16_t id, EtherAddress *node) {
        uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
        struct flooding_last_node *fln = _assigned_node_list[index];

        if ((fln == NULL) || (_bcast_id_list[index] != id)) return;

        int fln_i = _assigned_node_list_size[index];

        //search for node
        for ( int i = 0; i < fln_i; i++ )
          if ( memcmp(node->data(), fln[i].etheraddr, 6) == 0 ) {
            fln[i].flags &= ~((uint8_t)FLOODING_LAST_NODE_FLAGS_REVOKE_ASSIGN);
            break;
          }
      }

      /**
       * Responsibility
       */

      void set_responsibility_target(uint16_t id, EtherAddress *target) {
        struct flooding_last_node* ln = get_last_node(id, target);
        if ( ln != NULL ) {
          ln->flags |= FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY;
          ln->flags &= ~FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY; //clear foreign_responsibility
        } else {
          add_last_node(id, target, false, false, true, false);
        }
      }

      void set_foreign_responsibility_target(uint16_t id, EtherAddress *target) {
        struct flooding_last_node* ln = get_last_node(id, target);
        if ( ln != NULL ) {
          ln->flags &= ~FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY;
          ln->flags |= FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY; //set foreign_responsibility
        } else {
          add_last_node(id, target, false, false, false, true);
        }
      }

      bool is_responsibility_target(uint16_t id, EtherAddress *target) {
        struct flooding_last_node* ln = get_last_node(id, target);
        if ( ln == NULL ) return false;

        return ((ln->flags & FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY) != 0);
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

  BroadcastNode *add_broadcast_node(EtherAddress *src);
  BroadcastNode *get_broadcast_node(EtherAddress *src);

  void add_id(EtherAddress* src, uint16_t id, Timestamp* now, bool = false);
  void inc_received(EtherAddress *src, uint16_t id, EtherAddress *last_node);
  bool have_id(EtherAddress *src, uint16_t id, Timestamp *now, uint32_t *fwd_attempts);

  void forward_done(EtherAddress *src, uint16_t id, bool success, bool new_node = false);
  void forward_attempt(EtherAddress *src, uint16_t id);
  uint32_t unfinished_forward_attempts(EtherAddress *src, uint16_t id);
  void sent(EtherAddress *src, uint16_t id, uint32_t no_transmission);

  int add_last_node(EtherAddress *src, uint16_t id, EtherAddress *last_node, bool forwarded, bool rx_acked, bool responsibility, bool foreign_responsibility);
  struct Flooding::BroadcastNode::flooding_last_node* get_last_nodes(EtherAddress *src, uint16_t id, uint32_t *size);
  struct Flooding::BroadcastNode::flooding_last_node* get_last_node(EtherAddress *src, uint16_t id, EtherAddress *last);

  bool me_src(EtherAddress *src, uint16_t id);

  void assign_last_node(EtherAddress *src, uint16_t id, EtherAddress *last_node);
  struct Flooding::BroadcastNode::flooding_last_node* get_assigned_nodes(EtherAddress *src, uint16_t id, uint32_t *size);
  void clear_assigned_nodes(EtherAddress *src, uint16_t id);

  void set_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target);
  void set_foreign_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target);
  bool is_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target);

  int retransmit_broadcast(Packet *p, EtherAddress *src, uint16_t bcast_id);

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

  typedef HashMap<EtherAddress, uint32_t> RecvCntMap;
  typedef RecvCntMap::const_iterator RecvCntMapIter;
  RecvCntMap _recv_cnt;

  uint8_t extra_data[BCAST_MAX_EXTRA_DATA_SIZE];
  uint32_t extra_data_size;

  bool _passive_last_node_new;
  bool _passive_last_node_rx_acked;
  bool _passive_last_node_assign;
  bool _passive_last_node_foreign_responsibility;

 public:

  bool is_local_addr(EtherAddress *ea) { return _me->isIdentical(ea);}

  uint32_t _flooding_src;
  uint32_t _flooding_rx;
  uint32_t _flooding_sent;
  uint32_t _flooding_fwd;
  uint32_t _flooding_passive;
  uint32_t _flooding_passive_not_acked_dst;
  uint32_t _flooding_passive_not_acked_force_dst;
  uint32_t _flooding_last_node_due_to_passive;
  uint32_t _flooding_last_node_due_to_ack;
  uint32_t _flooding_last_node_due_to_piggyback;
  uint32_t _flooding_lower_layer_reject;
  uint32_t _flooding_src_new_id;
  uint32_t _flooding_rx_new_id;
  uint32_t _flooding_fwd_new_id;
  uint32_t _flooding_rx_ack;


  /* Members and functions for tx abort */
#define FLOODING_TXABORT_MODE_NONE        0
#define FLOODING_TXABORT_MODE_ACKED       1
#define FLOODING_TXABORT_MODE_ASSIGNED    2
#define FLOODING_TXABORT_MODE_BETTER_LINK 4
#define FLOODING_TXABORT_MODE_NEW_INFO    8

#define FLOODING_TXABORT_REASON_NONE                    FLOODING_TXABORT_MODE_NONE
#define FLOODING_TXABORT_REASON_ACKED                   FLOODING_TXABORT_MODE_ACKED
#define FLOODING_TXABORT_REASON_ASSIGNED                FLOODING_TXABORT_MODE_ASSIGNED
#define FLOODING_TXABORT_REASON_BETTER_LINK             FLOODING_TXABORT_MODE_BETTER_LINK
#define FLOODING_TXABORT_REASON_NEW_INFO                FLOODING_TXABORT_MODE_NEW_INFO
#define FLOODING_TXABORT_REASON_FOREIGN_RESPONSIBILITY  FLOODING_TXABORT_MODE_ACKED       /* Take foreign resp. as acked */


  uint32_t _abort_tx_mode;
  uint32_t _tx_aborts;
  uint32_t _tx_aborts_errors;

 private:

  EtherAddress _last_tx_dst_ea;
  EtherAddress _last_tx_src_ea;
  uint16_t _last_tx_bcast_id;
  bool _last_tx_abort;

 public:

  inline void set_last_tx(EtherAddress dst, EtherAddress src, uint16_t id) {
    _last_tx_dst_ea = dst;
    _last_tx_src_ea = src;
    _last_tx_bcast_id = id;
  };

  void reset_last_tx() { set_last_tx(EtherAddress(), EtherAddress(), 0); _last_tx_abort = false; }

  inline bool is_last_tx_id(EtherAddress &src, uint16_t id) {
    return (_last_tx_src_ea == src) && (_last_tx_bcast_id == id);
  }

  bool is_last_tx(EtherAddress &dst, EtherAddress &src, uint16_t id) {
    return ((_last_tx_dst_ea == dst) && is_last_tx_id(src, id));
  }

  void abort_last_tx(EtherAddress &dst, uint32_t abort_reason) {
    if (((_abort_tx_mode & abort_reason) == FLOODING_TXABORT_MODE_NONE) || dst.is_broadcast()) return;

    BRN_DEBUG("Abort: %s %s %d (%s)", _last_tx_dst_ea.unparse().c_str(),
                                      _last_tx_src_ea.unparse().c_str(),
                                      _last_tx_bcast_id, dst.unparse().c_str());

    if (_last_tx_abort ) {
      BRN_WARN("Abort already running");
      return;
    }

    bool failure = false;
    for (int devs = 0; devs < _me->countDevices(); devs++)
      failure |= (_me->getDeviceByIndex(devs)->abort_transmission(dst) != 0);

    if ( failure ) {
      _tx_aborts_errors++;
    } else {
      BRN_DEBUG("Abort last TX");
      _tx_aborts++;
      _last_tx_abort = true;
    }
  }

  inline void abort_last_tx(uint32_t abort_reason) {
    abort_last_tx(_last_tx_dst_ea, abort_reason);
  }

};

CLICK_ENDDECLS
#endif
