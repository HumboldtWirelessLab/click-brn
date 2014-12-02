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

#ifndef FLOODINGDBELEMENT_HH
#define FLOODINGDBELEMENT_HH

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

CLICK_DECLS


/*
 * =c
 * FloodingDB()
 * =s
 * 
 * =d
 */

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
#define FLOODING_NODE_INFO_LIST_DFLT_SIZE     16

  public:
  EtherAddress  _src;

  uint16_t _bcast_id_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
  uint8_t _bcast_fwd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];      //no fwd paket
  uint8_t _bcast_fwd_done_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //no fwd done paket (fwd-fwd_done=no_queued_packets
  uint8_t _bcast_fwd_succ_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE]; //no packet recv by at least on node
  uint8_t _bcast_snd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];      //no transmission (incl. retries for unicast)
  uint8_t _bcast_rts_snd_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];  //no transmission (incl. retries for unicast)
  uint8_t _bcast_flags_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];    //flags
  Timestamp _bcast_time_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];   //time

#define FLOODING_FLAGS_ME_SRC             1
#define FLOODING_FLAGS_TX_ABORT           2
#define FLOODING_FLAGS_ME_UNICAST_TARGET  4 /* as long as i'm not unicast target and did not forward, nobody knows that i have received it */

  Timestamp _last_id_time;           //timeout for hole queue TODO: remove since its deprecated

  //stats for last node of one packet
  struct flooding_node_info {
    uint8_t etheraddr[6];

    uint8_t rx_probability;
    uint8_t received_cnt;

    uint8_t tx_count;
    uint8_t flags;

#define FLOODING_NODE_INFO_FLAGS_FORWARDED                    1
#define FLOODING_NODE_INFO_FLAGS_FINISHED                     2
#define FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY               4
#define FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY       8

#define FLOODING_NODE_INFO_FLAGS_FINISHED_FOR_ME              (FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY | FLOODING_NODE_INFO_FLAGS_FINISHED)
#define FLOODING_NODE_INFO_FLAGS_FINISHED_FOR_FOREIGN         (FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY | FLOODING_NODE_INFO_FLAGS_FINISHED)

#define FLOODING_NODE_INFO_FLAGS_FINISHED_RESPONSIBILITY      16

#define FLOODING_NODE_INFO_FLAGS_IS_ASSIGNED_NODE             32

/* I'm already respon. but i received a packet with foreign_responsibility */
#define FLOODING_NODE_INFO_FLAGS_GUESS_FOREIGN_RESPONSIBILITY 64

#define FLOODING_NODE_INFO_FLAGS_NODE_WAS_UNICAST_TARGET     128
  };


#define FLOODING_NODE_INFO_RESULT_IS_NEW                         1
#define FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED                2
#define FLOODING_NODE_INFO_RESULT_IS_NEW_FOREIGN_RESPONSIBILITY  4


  struct flooding_node_info *_flooding_node_info_list[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
  uint8_t _flooding_node_info_list_size[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];
  uint8_t _flooding_node_info_list_maxsize[DEFAULT_MAX_BCAST_ID_QUEUE_SIZE];

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

    memset(_flooding_node_info_list,0,sizeof(_flooding_node_info_list));
    memset(_flooding_node_info_list_maxsize,0,sizeof(_flooding_node_info_list_maxsize));
  }

  void reset_queue() {
    memset(_bcast_id_list, 0, sizeof(_bcast_id_list));
    memset(_bcast_fwd_list, 0, sizeof(_bcast_fwd_list));
    memset(_bcast_fwd_done_list, 0, sizeof(_bcast_fwd_done_list));
    memset(_bcast_fwd_succ_list, 0, sizeof(_bcast_fwd_succ_list));
    memset(_bcast_snd_list, 0, sizeof(_bcast_snd_list));
    memset(_bcast_rts_snd_list, 0, sizeof(_bcast_rts_snd_list));
    memset(_bcast_flags_list, 0, sizeof(_bcast_flags_list));

    memset(_flooding_node_info_list_size,0,sizeof(_flooding_node_info_list_size));                      //all entries are invalid
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
      _bcast_rts_snd_list[index] = 0;
      _bcast_flags_list[index] = (me_src)?FLOODING_FLAGS_ME_SRC:0;
      _bcast_time_list[index] = now;

      _flooding_node_info_list_size[index] = 0;
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
    if (_bcast_id_list[index] == id) _bcast_fwd_list[index]++;
  }

  inline void forward_done(uint16_t id, bool success) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] == id) {
      _bcast_fwd_done_list[index]++;
      if (success) _bcast_fwd_succ_list[index]++;
    }
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

  inline int forward_done_cnt(uint16_t id) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] == id) return _bcast_fwd_done_list[index];
    return -1;
  }

  inline void sent(uint16_t id, uint32_t no_transmissions, uint32_t no_rts_transmissions) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] == id) {
      _bcast_snd_list[index] += no_transmissions;
      _bcast_rts_snd_list[index] += no_rts_transmissions;
    }
  }

  inline int get_sent(uint16_t id) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    return (_bcast_id_list[index] == id)?_bcast_snd_list[index]:0;
  }

  /**
    * Known nodes (node info)
    * 
    * 
    */

  struct flooding_node_info *add_node_info(uint16_t id, EtherAddress *node, int *new_index)
  {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    assert(_bcast_id_list[index] == id);

    /* create or extend the queue of nodes*/
    if (_flooding_node_info_list[index] == NULL) {
      _flooding_node_info_list[index] = new struct flooding_node_info[FLOODING_NODE_INFO_LIST_DFLT_SIZE];
      _flooding_node_info_list_size[index] = 0;
      _flooding_node_info_list_maxsize[index] = FLOODING_NODE_INFO_LIST_DFLT_SIZE;
    } else {
      if ( _flooding_node_info_list_size[index] == _flooding_node_info_list_maxsize[index] ) {
        _flooding_node_info_list_maxsize[index] *= 2; //double
        struct flooding_node_info *new_list = new struct flooding_node_info[_flooding_node_info_list_maxsize[index]]; //new list
        memcpy(new_list,_flooding_node_info_list[index], _flooding_node_info_list_size[index] * sizeof(struct flooding_node_info)); //copy
        delete[] _flooding_node_info_list[index]; //delete old
        _flooding_node_info_list[index] = new_list; //set new
      }
    }

    /* add the node or update */
    struct flooding_node_info *flni = _flooding_node_info_list[index];
    int flni_s = _flooding_node_info_list_size[index];

    for ( int i = 0; i < flni_s; i++ ) {
      if ( memcmp(node->data(), flni[i].etheraddr, 6) == 0 ) {               //found node
         *new_index = 0;                                                     //found node, so new index is 0
         return &(flni[i]);
      }
    }

    /*
     * Node not found, so add new_list
     */
    memcpy(flni[flni_s].etheraddr, node->data(),6);
    flni[flni_s].received_cnt = flni[flni_s].rx_probability = flni[flni_s].tx_count = flni[flni_s].flags = 0;

    _flooding_node_info_list_size[index]++;
    *new_index = _flooding_node_info_list_size[index]; //after inc to avoid return of 0 for the first node

    return &(flni[flni_s]);

  }

  int add_node_info(uint16_t id, EtherAddress *node, bool forwarded, bool finished, bool responsibility, bool foreign_responsibility) {
    int result = 0;

    struct flooding_node_info *fln = add_node_info(id, node, &result);

    if ( result == 0 ) {                                                    //found node
      if ( forwarded ) fln->flags |= FLOODING_NODE_INFO_FLAGS_FORWARDED;
      if ( finished ) {
        if ( ! (fln->flags & FLOODING_NODE_INFO_FLAGS_FINISHED) ) {         //is not finished yet

          result |= FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED;

          fln->flags |= FLOODING_NODE_INFO_FLAGS_FINISHED;                  //is acked ? so mark it
          if ( fln->flags & FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY ) {     //i was resp?
            fln->flags |= FLOODING_NODE_INFO_FLAGS_FINISHED_RESPONSIBILITY; //then mark it as finished
          }

          fln->flags &= ~(FLOODING_NODE_INFO_FLAGS_IS_ASSIGNED_NODE);

          fln->rx_probability = 100;
        }
      } else {                                                              //this node is already know
        if ( responsibility ) {
          assert((fln->flags & FLOODING_NODE_INFO_FLAGS_FINISHED) == 0);    //if i set responsible it should not finished yet
          fln->flags |= FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY;            //set responsibility
          fln->flags &= ~FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY;   //clear foreign_responsibility
        } else if (foreign_responsibility &&                                //set foreign_responsibility only if ...
                   ((fln->flags & FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY) != 0)) { //not set yet

                if ((fln->flags & FLOODING_NODE_INFO_FLAGS_FINISHED_FOR_FOREIGN) == 0) {  //or i'm not respo and its not finished so

                  fln->flags |= FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY;          //set flag
                  result |= FLOODING_NODE_INFO_RESULT_IS_NEW_FOREIGN_RESPONSIBILITY;

                } else if ((fln->flags & FLOODING_NODE_INFO_FLAGS_FINISHED) == 0) {       //its finished for foreign but not
                                                                                          //finished, since i'm responsible
                  fln->flags |= FLOODING_NODE_INFO_FLAGS_GUESS_FOREIGN_RESPONSIBILITY;    //set guess foreign resp
                                                                                          //TODO: if no packet sent yet, we can remove our 
                                                                                          //responsibility
                }
        }
      }
    } else {

      /*
      * Node not found, so add new_list
      */
      result = FLOODING_NODE_INFO_RESULT_IS_NEW;                          //this node is new

      if ( forwarded ) fln->flags |= FLOODING_NODE_INFO_FLAGS_FORWARDED;

      if ( finished ) {

        result |= FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED;

        fln->flags |= FLOODING_NODE_INFO_FLAGS_FINISHED;
        fln->rx_probability = 100;
      } else if ( responsibility ) fln->flags |= FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY;
        else if ( foreign_responsibility ) {
          fln->flags |= FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY;
          result |= FLOODING_NODE_INFO_RESULT_IS_NEW_FOREIGN_RESPONSIBILITY;
        }
    }

    return result;
  }

  struct flooding_node_info* get_node_info(uint16_t id, EtherAddress *last) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

    if ((_flooding_node_info_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;

    struct flooding_node_info *fln = _flooding_node_info_list[index];
    int fln_i = _flooding_node_info_list_size[index];

    //search for node
    for ( int i = 0; i < fln_i; i++ ) {
      if ( memcmp(last->data(), fln[i].etheraddr, 6) == 0 ) return &fln[i];
    }

    return NULL;
  }

  struct flooding_node_info* get_node_infos(uint16_t id, uint32_t *size) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;

    *size = 0;
    if ((_flooding_node_info_list[index] == NULL) || (_bcast_id_list[index] != id)) return NULL;
    *size = (uint32_t)( _flooding_node_info_list_size[index]);
    return _flooding_node_info_list[index];
  }

  inline void add_recv_last_node(uint16_t id, EtherAddress *last) {
    struct flooding_node_info *fln = get_node_info(id, last);
    if ( fln != NULL ) fln->received_cnt++;
  }

  inline int add_responsible_node(uint16_t id, EtherAddress *node) {
    int new_index = 0;

    struct flooding_node_info *fln = add_node_info(id, node, &new_index);
    fln->flags |= FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY;               //set responsibility

    if ( new_index ) return FLOODING_NODE_INFO_RESULT_IS_NEW;
    return 0;
  }

  inline void set_tx_count_last_node(uint16_t id, EtherAddress *last, uint8_t tx_count) {
    struct flooding_node_info *fln = get_node_info(id, last);
    if ( fln != NULL ) fln->tx_count = tx_count;
  }

  /**
    * ASSIGN: flags whether the packet is transmited to a node A (LastNode) by another node
    */

  int assign_node(uint16_t id, EtherAddress *node) {
    int new_index = 0;
    struct flooding_node_info *fln = add_node_info(id, node, &new_index);

    if ((fln->flags & FLOODING_NODE_INFO_FLAGS_FINISHED) != 0) return 0; //node already finished

    fln->flags |= FLOODING_NODE_INFO_FLAGS_IS_ASSIGNED_NODE;

    return new_index;
  }

  /* clear all assign nodes */
  void clear_assigned_nodes(uint16_t id) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] == id) {
      struct flooding_node_info *fln = _flooding_node_info_list[index];
      int fln_i = _flooding_node_info_list_size[index];

      //search for node
      for ( int i = 0; i < fln_i; i++ )
        fln[i].flags &= ~((uint8_t)FLOODING_NODE_INFO_FLAGS_IS_ASSIGNED_NODE);
    }
  }

  /* clear single assign nodes */
  void revoke_assigned_node(uint16_t id, EtherAddress *node) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    struct flooding_node_info *fln = _flooding_node_info_list[index];

    if ((fln == NULL) || (_bcast_id_list[index] != id)) return;

    int fln_i = _flooding_node_info_list_size[index];

    //search for node
    for ( int i = 0; i < fln_i; i++ )
      if ( memcmp(node->data(), fln[i].etheraddr, 6) == 0 ) {
        fln[i].flags &= ~((uint8_t)FLOODING_NODE_INFO_FLAGS_IS_ASSIGNED_NODE);
        break;
      }
  }

  /**
    * Responsibility
    */

  void set_responsibility_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    if ( ln != NULL ) {
      ln->flags |= FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY;
      ln->flags &= ~FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY; //clear foreign_responsibility
    } else {
      add_node_info(id, target, false, false, true, false);
    }
  }

  void set_foreign_responsibility_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    if ( ln != NULL ) {
      ln->flags |= FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY; //set foreign_responsibility
    } else {
      add_node_info(id, target, false, false, false, true);
    }
  }

  bool is_responsibility_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    if ( ln == NULL ) return false;

    return ((ln->flags & FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY) != 0);
  }

  bool is_foreign_responsibility_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    if ( ln == NULL ) return false;

    return ((ln->flags & FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY) != 0);
  }

  bool guess_foreign_responsibility_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    if ( ln == NULL ) return false;

    return ((ln->flags & FLOODING_NODE_INFO_FLAGS_GUESS_FOREIGN_RESPONSIBILITY) != 0);
  }

  void clear_responsibility_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    if ( ln != NULL ) {
      ln->flags &= ~FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY; //clear responsibility
    }
  }
  /**
    * TX-Abort
    */

  void set_stopped(uint16_t id, bool stop) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] != id) return;

    if (stop) _bcast_flags_list[index] |= FLOODING_FLAGS_TX_ABORT;
    else _bcast_flags_list[index] &= (uint8_t)~FLOODING_FLAGS_TX_ABORT;
  }

  bool is_stopped(uint16_t id) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] != id) return false;

    return ((_bcast_flags_list[index]&FLOODING_FLAGS_TX_ABORT) == FLOODING_FLAGS_TX_ABORT);
  }

  /**
   *  Unicast target
   */

  void set_me_as_unicast_target(uint16_t id) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] != id) return;
    _bcast_flags_list[index] |= FLOODING_FLAGS_ME_UNICAST_TARGET;
  }


  bool me_was_unicast_target(uint16_t id) {
    uint16_t index = id & DEFAULT_MAX_BCAST_ID_QUEUE_SIZE_MASK;
    if (_bcast_id_list[index] != id) return false;
    return ((_bcast_flags_list[index] & FLOODING_FLAGS_ME_UNICAST_TARGET) != 0);
  }

  void set_node_as_unicast_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    assert(ln!=NULL);
    ln->flags |= FLOODING_NODE_INFO_FLAGS_NODE_WAS_UNICAST_TARGET;
  }

  bool node_was_unicast_target(uint16_t id, EtherAddress *target) {
    struct flooding_node_info* ln = get_node_info(id, target);
    assert(ln!=NULL);
    return ((ln->flags & FLOODING_NODE_INFO_FLAGS_NODE_WAS_UNICAST_TARGET)!=0);
  }

  /**
   * Probability
   */

  void add_probability(uint16_t id, EtherAddress *node, uint32_t probability, uint32_t no_transmission) {
    int new_index = 0;
    struct flooding_node_info *fln = add_node_info(id, node, &new_index);

    for ( uint32_t i = 0; i < no_transmission; i++ ) //MIN to avoid overflow (prob > 100)
      fln->rx_probability = MIN(100, (uint32_t)fln->rx_probability + ((((uint32_t)100 - (uint32_t)fln->rx_probability) * probability) / (uint32_t)100));

    //TODO: use 1-pow(1-probability,no_transmission) as final succ transmission probability
  }

  int get_probability(uint16_t id, EtherAddress *node) {
    int new_index = 0;
    struct flooding_node_info *fln = add_node_info(id, node, &new_index);

    return fln->rx_probability;
  }
};

typedef HashMap<EtherAddress, BroadcastNode*> BcastNodeMap;
typedef BcastNodeMap::const_iterator BcastNodeMapIter;

typedef HashMap<EtherAddress, uint32_t> RecvCntMap;
typedef RecvCntMap::const_iterator RecvCntMapIter;

class FloodingDB : public BRNElement {

 public:

  //
  //methods
  //
  FloodingDB();
  ~FloodingDB();

  const char *class_name() const  { return "FloodingDB"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  BroadcastNode *add_broadcast_node(EtherAddress *src);
  BroadcastNode *get_broadcast_node(EtherAddress *src);

  void add_id(EtherAddress* src, uint16_t id, Timestamp* now, bool = false);
  void inc_received(EtherAddress *src, uint16_t id, EtherAddress *last_node);
  bool have_id(EtherAddress *src, uint16_t id, Timestamp *now, uint32_t *fwd_attempts);

  void forward_done(EtherAddress *src, uint16_t id, bool success);
  uint32_t forward_done_cnt(EtherAddress *src, uint16_t id);
  void forward_attempt(EtherAddress *src, uint16_t id);
  uint32_t unfinished_forward_attempts(EtherAddress *src, uint16_t id);
  void sent(EtherAddress *src, uint16_t id, uint32_t no_transmission, uint32_t no_rts_transmission);

  int add_node_info(EtherAddress *src, uint16_t id, EtherAddress *last_node, bool forwarded, bool rx_acked, bool responsibility, bool foreign_responsibility);
  int add_responsible_node(EtherAddress *src, uint16_t id, EtherAddress *last_node);

  /**
   * @brief Set the number of sent packets. This information is collected and send by by the node "last_node"
   * 
   * @param src source of the flooding (this information is about)
   * @param id id of flooding (this information is about)
   * @param last_node last node (the node sentthe flooding tx_count times)
   * @param tx_count number of transmission by last_node
   * @return void
   */
  void set_tx_count_last_node(EtherAddress *src, uint16_t id, EtherAddress *last_node, uint8_t tx_count);

  struct BroadcastNode::flooding_node_info* get_node_infos(EtherAddress *src, uint16_t id, uint32_t *size);
  struct BroadcastNode::flooding_node_info* get_node_info(EtherAddress *src, uint16_t id, EtherAddress *last);

  bool me_src(EtherAddress *src, uint16_t id);

  void assign_last_node(EtherAddress *src, uint16_t id, EtherAddress *last_node);
  void clear_assigned_nodes(EtherAddress *src, uint16_t id);

  void set_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target);
  void set_foreign_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target);
  bool is_responsibility_target(EtherAddress *src, uint16_t id, EtherAddress *target);

  String table();
  void reset();

 public:
  //
  //member
  //
  BcastNodeMap _bcast_map;

  RecvCntMap _recv_cnt;
};

CLICK_ENDDECLS
#endif
