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
#include "elements/brn/common.hh"
#include "brn2_dsrdecap.hh"
#include "brn2_dsrencap.hh"
#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

#include <click/element.hh>
#include <click/bighashmap.hh>

CLICK_DECLS

class BRN2NodeIdentity;
class BRN2RouteQuerier;
class BRN2DSREncap;

#define JITTER 17
#define MIN_JITTER 5


/*
 * =c
 * BRN2RequestForwarder()
 * =s forwards rreq packets
 * forwards dsr route request packets
 * output 0: route reply
 * output 1: route request
 * =d
 */
class BRN2RequestForwarder : public Element {

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
       _send_time = Timestamp::now().timeval();
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
  void add_handlers();

  void forward_rreq(Packet *p_in);
  void add_route_to_link_table(const BRN2RouteQuerierRoute &);

  void run_timer(Timer *timer);

public: 
  //
  //member
  //
  int _debug;
  BRN2NodeIdentity *_me;
  Brn2LinkTable *_link_table;
  BRN2DSRDecap *_dsr_decap;
  BRN2DSREncap *_dsr_encap;
  BRN2Encap *_brn_encap;
  BRN2RouteQuerier *_route_querier;


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
  void reverse_route(const BRN2RouteQuerierRoute &in, BRN2RouteQuerierRoute &out);
  void issue_rrep(EtherAddress, IPAddress, EtherAddress, IPAddress, const BRN2RouteQuerierRoute &, uint16_t rreq_id);
  int findOwnIdentity(const BRN2RouteQuerierRoute &r);
};

CLICK_ENDDECLS
#endif
