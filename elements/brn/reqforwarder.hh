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

#ifndef REQUESTFORWARDERELEMENT_HH
#define REQUESTFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/timer.hh>
#include "common.hh"
#include "dsrdecap.hh"
#include "dsrencap.hh"
#include "assoclist.hh"
#include "nodeidentity.hh"

#include <click/element.hh>
#include <click/bighashmap.hh>

CLICK_DECLS

class NodeIdentity;
class RouteQuerier;
class DSREncap;
class BrnIappStationTracker;

#define JITTER 17
#define MIN_JITTER 5


/*
 * =c
 * RequestForwarder()
 * =s forwards rreq packets
 * forwards dsr route request packets
 * output 0: route reply
 * output 1: route request
 * =d
 */
class RequestForwarder : public Element {

 public:

  class BufferedPacket
  {
    public:
     Packet *_p;
     struct timeval _send_time;

     BufferedPacket(Packet *p, int time_diff)
     {
       assert(p);
       _p=p;
       click_gettimeofday(&_send_time);
       _send_time.tv_sec += ( time_diff / 1000 );
       _send_time.tv_usec += ( ( time_diff % 1000 ) * 1000 );
       while( _send_time.tv_usec >= 1000000 )  //handle timeoverflow
       {
         _send_time.tv_usec -= 1000000;
         _send_time.tv_sec++;
       }
     }
     void check() const { assert(_p); }
  };

  typedef Vector<BufferedPacket> SendBuffer;

  //
  //methods
  //
  RequestForwarder();
  ~RequestForwarder();

  const char *class_name() const  { return "RequestForwarder"; }
  const char *port_count() const  { return "1/2"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return false; }

  void push(int, Packet *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void forward_rreq(Packet *p_in);
  void add_route_to_link_table(const RouteQuerierRoute &);

  void run_timer(Timer *timer);

public: 
  //
  //member
  //
  int _debug;
  NodeIdentity *_me;
  BrnLinkTable *_link_table;
  DSRDecap *_dsr_decap;
  DSREncap *_dsr_encap;
  BRNEncap *_brn_encap;
  AssocList *_client_assoc_lst;
  RouteQuerier *_route_querier;
  BrnIappStationTracker* _iapp;

private:
  int _min_metric_rreq_fwd;

// keep track of already forwarded RREQ; forward only route request with better metric
// TODO mal ordentlich machen (Timeout)
  HashMap<uint32_t, int> _track_route;

  //
  //Timer methods
  SendBuffer _packet_queue;
  Timer _sendbuffer_timer;

  long diff_in_ms(timeval t1, timeval t2);
  void queue_timer_hook();
  unsigned int get_min_jitter_in_queue();

  //
  //methods
  //
  void reverse_route(const RouteQuerierRoute &in, RouteQuerierRoute &out);
  void issue_rrep(EtherAddress, IPAddress, EtherAddress, IPAddress, const RouteQuerierRoute &, uint16_t rreq_id);
};

CLICK_ENDDECLS
#endif
