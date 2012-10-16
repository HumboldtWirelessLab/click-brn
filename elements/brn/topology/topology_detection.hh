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

#ifndef TOPOLOGY_DETECTION_HH
#define TOPOLOGY_DETECTION_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include <clicknet/ether.h>

#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

#include "elements/brn/brnelement.hh"

#include "topology_info.hh"

CLICK_DECLS

/*
 *=c
 *TopologyDetection()
 *=s
*/
class TopologyDetection : public BRNElement {

 public:

   class TopologyDetectionReceivedInfo {
    public:
     EtherAddress _addr;
     uint32_t _ttl;
     bool _over_me;
     bool _descendant;

     TopologyDetectionReceivedInfo(EtherAddress *addr, uint32_t ttl, bool over_me) {
       _addr = EtherAddress(addr->data());
       _ttl = ttl;
       _over_me = over_me;
       _descendant = false;
     }
   };

   class TopologyDetectionForwardInfo {
    public:
      EtherAddress _src;
      uint32_t _id;
      Timestamp _first_seen;
      Timestamp _last_seen;
      uint32_t _get_backward;
      uint32_t _num_descendant;
      uint8_t _ttl;

      Vector<TopologyDetectionReceivedInfo> _last_hops;

      TopologyDetectionForwardInfo(uint8_t *src_p, uint32_t id) {
        _id = id;
        _last_seen = Timestamp::now();
        _src = EtherAddress(src_p);
        _get_backward = 0;
        _num_descendant = 0;
      }

      TopologyDetectionForwardInfo(const EtherAddress *src, uint32_t id, uint8_t ttl) {
        _id = id;
        _last_seen = Timestamp::now();
        _src = *src;
        _get_backward = 0;
        _num_descendant = 0;
        _ttl = ttl;
      }

      void update() {
        _last_seen = Timestamp::now();
      }

      bool equals(const EtherAddress *src, uint32_t id) {
        return ( (_src == *src) && (_id==id));
      }

      /* since each node forward each message only one time, this function
         doesn't have to check, whether node is already in list */
      void add_last_hop(EtherAddress *lh, uint32_t ttl, bool over_me) {
        _last_hops.push_back(TopologyDetectionReceivedInfo(lh,ttl,over_me));
      }

      bool include_last_hop(EtherAddress *lh) {
        for( int i = 0; i < _last_hops.size(); i++ )
          if ( _last_hops[i]._addr == *lh ) return true;
        return false;
      }

      void set_descendant(EtherAddress *lh, bool desc) {
        for( int i = 0; i < _last_hops.size(); i++ )
          if ( _last_hops[i]._addr == *lh ) {
            _last_hops[i]._descendant = desc;
            return;
          }
      }

      /*node receives only one message
        -link to parent is bridge */
      bool pendant_node() {
        return (_num_descendant == 0);
      }

      /*node recieves msg with same ttl as sending ttl
        - links to node with same ttl are non-bridges */
      bool odd_loop(Vector<EtherAddress> */*loop_nodes*/) {
        return false;
      }

      /*node receives two or more message with same ttl which is higher than sending ttl
        - nodes are non-bridges*/
      bool even_loop(Vector<EtherAddress> */*loop_nodes*/) {
        return false;
      }

  };

  typedef Vector<TopologyDetectionForwardInfo*> TDFIList;

 public:
  //
  //methods
  //
  TopologyDetection();
  ~TopologyDetection();

  const char *class_name() const  { return "TopologyDetection"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  int initialize(ErrorHandler *);

  void add_handlers();

  void push( int port, Packet *packet );

  static void static_detection_timer_hook(Timer *t, void *f);
  static void static_response_timer_hook(Timer *t, void *f);

  void start_detection();

  Timer _detection_timer;
  Timer _response_timer;

  TDFIList tdfi_list;

 private:
  //
  //member
  //

  uint32_t detection_id;

  Brn2LinkTable *_lt;
  BRN2NodeIdentity *_node_identity;
  TopologyInfo *_topoi;

  void handle_detection_backward(Packet *packet);
  void handle_detection_forward(Packet *packet);

  void handle_detection_timeout(void);
  void handle_response_timeout(void);

  void send_response(void);

  TopologyDetectionForwardInfo *get_forward_info(EtherAddress *src, uint32_t id);
  bool path_include_node(uint8_t *path, uint32_t path_len, const EtherAddress *node );

 public:
  String local_topology_info(void);
};

CLICK_ENDDECLS
#endif
