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

#ifndef BRN2REQUESTFORWARDERELEMENT_HH
#define BRN2REQUESTFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

#include "elements/brn2/wifi/ap/brn2_assoclist.hh"
#include "elements/brn2/standard/packetsendbuffer.hh"

#include "brn2_routequerier.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_dsrencap.hh"

CLICK_DECLS

#define JITTER 17
#define MIN_JITTER 5

class BRN2DSREncap;
class BRN2RouteQuerier;


/*
 * =c
 * BRN2RequestForwarder()
 * =s forwards rreq packets
 * forwards dsr route request packets
 * output 0: route reply
 * output 1: route request
 * =d
 */

#define METRIC_LIST_SIZE (uint32_t)64
#define METRIC_LIST_MASK METRIC_LIST_SIZE - 1
#define METRIC_INVALID 0xFFFF

#define PASSIVE_ACK_MAX_NEIGHBOURS 16
#define DEFAULT_REQUEST_MAX_AGE_MS 1000

class BRN2RequestForwarder : public BRNElement {

  class RouteRequestInfo {
   public:
    EtherAddress _src;
    uint16_t _id_list[METRIC_LIST_SIZE];
    uint16_t _metric_list[METRIC_LIST_SIZE];
    Timestamp _times_list[METRIC_LIST_SIZE];

    uint16_t _max_age;

    uint16_t _passive_ack_vector_list[METRIC_LIST_SIZE];
    uint16_t _passive_ack_retry_list[METRIC_LIST_SIZE];
    Timestamp _passive_ack_last_retry_list[METRIC_LIST_SIZE];

    uint8_t _last_hop_opt[METRIC_LIST_SIZE];

   public:

    RouteRequestInfo(EtherAddress *src, uint16_t max_age ) {
      _src = EtherAddress(src->data());
      memset(_id_list, 0, sizeof(_id_list));
      _id_list[0] = 1; //set entry 0 to 1 so that it is mark as unused (1 & METRIC_LIST_MASK) != 0)
      _max_age = max_age;
    }

    inline uint16_t get_current_metric(uint16_t req_id, Timestamp *now) {
      uint16_t i = req_id & METRIC_LIST_MASK;
      if ( (_id_list[i] != req_id) ||
           ((*now - _times_list[req_id & METRIC_LIST_MASK]).msecval() > _max_age)) {
        return METRIC_INVALID;
      }
      return _metric_list[i];
    }

    inline void set_metric(uint16_t req_id, uint16_t metric, Timestamp *now, uint8_t last_hop_opt = 0) {
      uint16_t i = req_id & METRIC_LIST_MASK;
      _id_list[i] = req_id;
      _metric_list[i] = metric;
      _times_list[i] = *now;
      _last_hop_opt[i] = last_hop_opt;
    }

    inline void reset_passive_ack(uint16_t req_id, uint16_t neighbour_count, uint16_t max_retries) {
      uint16_t i = req_id & METRIC_LIST_MASK;
      _passive_ack_retry_list[i] = max_retries;
      _passive_ack_vector_list[i] = (uint16_t)((((uint32_t)1) << neighbour_count) - 1);
    }

    inline uint16_t left_retries(uint16_t req_id) {
      return _passive_ack_retry_list[req_id & METRIC_LIST_MASK];
    }

    inline void dec_retries(uint16_t req_id) {
      _passive_ack_retry_list[req_id & METRIC_LIST_MASK]--;
    }

    inline void received_neighbour(uint16_t req_id, uint16_t neighbour) {
      _passive_ack_vector_list[req_id & METRIC_LIST_MASK] &= ~((uint16_t)(1 << neighbour));
    }

    inline bool has_neighbours_left(uint16_t req_id) {
      return _passive_ack_vector_list[req_id & METRIC_LIST_MASK] != 0;
    }

    inline bool includes_rreq(uint16_t req_id) {
      return (_id_list[req_id & METRIC_LIST_MASK] == req_id);
    }
  };

  typedef HashMap<EtherAddress, RouteRequestInfo*> TrackRouteMap;
  typedef TrackRouteMap::iterator TrackRouteMapIter;

  class RReqRetransmitInfo {
   public:
    Packet *_p;
    EtherAddress _src;
    uint16_t _rreq_id;

    RReqRetransmitInfo(Packet *p, EtherAddress *ea, uint16_t rreq_id) {
      if ( p == NULL ) {
        click_chatter("Insert NULL packet");
      }
      _p = p;
      _src = EtherAddress(ea->data());
      _rreq_id = rreq_id;
    }

    Packet* get_packet(bool clone) {
      if ( clone ) {
        Packet *r = _p->clone();
        return r;
      }

      Packet *ret = _p;
      _p = NULL;
      return ret;
    }
  };

  typedef Vector<RReqRetransmitInfo*> RReqRetransmitInfoList;
  typedef RReqRetransmitInfoList::const_iterator RReqRetransmitInfoListIter;

 public:
  //
  //methods
  //
  BRN2RequestForwarder();
  ~BRN2RequestForwarder();

  const char *class_name() const  { return "BRN2RequestForwarder"; }
  const char *port_count() const  { return "1/2"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void uninitialize();

  String get_trackroutemap();

  void add_handlers();

  void forward_rreq(Packet *p_in, EtherAddress *detour_nb, int detour_metric_last_nb, RouteRequestInfo *rri, uint16_t rreq_id);

  void run_timer(Timer *timer);

public: 
  //
  //member
  //
  BRN2NodeIdentity *_me;
  Brn2LinkTable *_link_table;
  BRN2DSRDecap *_dsr_decap;
  BRN2DSREncap *_dsr_encap;
  BRN2RouteQuerier *_route_querier;

  void rreq_retransmit_timer_hook();
  static void static_rreq_retransmit_timer_hook(Timer *t, void *f);
  void check_passive_ack(EtherAddress *last_node_addr, EtherAddress *src, uint16_t rreq_id);
  int retransmission_index(EtherAddress *ea, uint16_t rreq_id);



 private:
  int _min_metric_rreq_fwd;

  uint32_t _max_age;

  RReqRetransmitInfoList _rreq_retransmit_list;
  Timer _retransmission_timer;

  Vector<EtherAddress> _neighbors;
  Timestamp _last_neighbor_update;

  TrackRouteMap _track_route_map;
  //
  //Timer methods
  PacketSendBuffer _packet_queue;
  Timer _sendbuffer_timer;
  void queue_timer_hook();

  //
  //methods
  //
  void reverse_route(const BRN2RouteQuerierRoute &in, BRN2RouteQuerierRoute &out);
  void issue_rrep(EtherAddress, IPAddress, EtherAddress, IPAddress, const BRN2RouteQuerierRoute &, uint16_t rreq_id);
  int findOwnIdentity(const BRN2RouteQuerierRoute &r);

  bool _enable_last_hop_optimization;
  bool _enable_full_route_optimization;

  bool _enable_delay_queue;

 public:
  int _stats_receive_better_route;
  int _stats_avoid_bad_route_forward;

  uint32_t _passive_ack_retries;
  uint32_t _passive_ack_interval;

  inline int32_t index_of_neighbor(EtherAddress *ea) {
    for ( int i = 0; i < _neighbors.size(); i++ ) if (_neighbors[i] == *ea) return i;
    return (int32_t)-1;
  }

};

CLICK_ENDDECLS
#endif
