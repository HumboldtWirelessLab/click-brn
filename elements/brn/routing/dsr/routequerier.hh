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

#ifndef ROUTEQUERIER_HH
#define ROUTEQUERIER_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "elements/brn/common.hh"
#include "elements/brn/routing/linkstat/brnlinktable.hh"
#include "brnencap.hh"
#include "dsrencap.hh"
#include "dsrdecap.hh"
#include "reqforwarder.hh"
#include <click/timer.hh>
#include "elements/brn/routing/linkstat/metric/genericmetric.hh"

CLICK_DECLS

class RouteQuerierHop;
class DSRDecap;
class DSREncap;
class BrnIappStationTracker;

typedef Vector<RouteQuerierHop> RouteQuerierRoute;
typedef Vector<EtherAddress> EtherAddresses;

/*
 * =c
 * RouteQuerier()
 * =s searches for a valid route for a given packet
 * if no route is available a rreq is generated; otherwise a dsr source routed packet will be created
 * output 0: route to destination is available
 * output 1: route to destination is not available
 * =d
 */
class RouteQuerier : public Element
{
public:
  typedef Pair<EtherAddress,EtherAddress> EtherPair;
  typedef HashMap<EtherPair,EtherAddresses> RouteMap;

  // Ethernet packets buffered while waiting for route replies
  class BufferedPacket
  {
  public:
    Packet *_p;
    struct timeval _time_added;

    BufferedPacket(Packet *p) {
      assert(p);
      _p=p;
      _time_added = Timestamp::now().timeval();
    }
    void check() const { assert(_p); }
  };

#define BRN_DSR_SENDBUFFER_MAX_LENGTH    20     // maximum number of packets to buffer per destination
#define BRN_DSR_SENDBUFFER_MAX_BURST      5     // maximum number of packets to send in one sendbuffer check
#define BRN_DSR_SENDBUFFER_TIMEOUT        1000  // how long packets can live in the sendbuffer (ms)
#define BRN_DSR_SENDBUFFER_TIMER_INTERVAL 3000  // how often to check for expired packets (ms)

  typedef Vector<BufferedPacket> SendBuffer;
  typedef HashMap<EtherAddress, SendBuffer> SBSourceMap;
  typedef HashMap<EtherAddress, SBSourceMap> SBMap;
  typedef SBMap::iterator SBMapIter;
  typedef SBSourceMap::iterator SBSourceMapIter;

  // blacklist of unidirectional links
  //
  // - when you receive a route reply, remove the link from the list
  // - when you forward a source-routed packet, remove the link from the list?
  // - when you forward a route request, drop it if it's unidirectionality is 'probable'
  // - if it's 'questionable', issue a RREQ with ttl 1
  // - entries go from probable -> questionable after some amount of time

#define BRN_DSR_BLACKLIST_NOENTRY            1
#define BRN_DSR_BLACKLIST_UNI_PROBABLE       2
#define BRN_DSR_BLACKLIST_UNI_QUESTIONABLE   3
#define BRN_DSR_BLACKLIST_UNITEST_TIMEOUT    1000 // ms
#define BRN_DSR_BLACKLIST_TIMER_INTERVAL     300 // how often to check for expired entries (ms)
#define BRN_DSR_BLACKLIST_ENTRY_TIMEOUT      45000 // how long until entries go from 'probable' to 'questionable'

  class BlacklistEntry {
  public:
    timeval _time_updated;
    int _status;

    void check() const {
      assert(_time_updated.tv_usec > 0);
      assert(_time_updated.tv_sec > 0);
      switch (_status) {
      case BRN_DSR_BLACKLIST_NOENTRY:
      case BRN_DSR_BLACKLIST_UNI_PROBABLE:
      case BRN_DSR_BLACKLIST_UNI_QUESTIONABLE:
	break;
      default: 
	assert(0);
      }
    }
  };
  typedef HashMap<EtherAddress, BlacklistEntry> Blacklist;
  typedef Blacklist::iterator BlacklistIter;


  // info about the last request we've originated to each target node.
  // these are kept in a hash table indexed by the target Ethernet.  when a
  // route reply is received from a host, we remove the entry.

  class InitiatedReq
  {
   public:

    EtherAddress _target;
    IPAddress _target_ip;
    EtherAddress _source;
    IPAddress _source_ip;

    int _ttl; // ttl used on the last request
    struct timeval _time_last_issued;

    // number of times we've issued a request to this target since
    // last receiving a reply
    unsigned int _times_issued; 

    // time from _time_last_issued until we can issue another, in ms
    unsigned long _backoff_interval;

  // first request to a new target has TTL1.  host waits
  // INITIAL_DELAY, if no response then it issues a new request with
  // TTL2, waits DELAY2.  subsequent requests have TTL2, but are
  // issued after multiplying the delay by the backoff factor.
#define BRN_DSR_RREQ_TTL1            255  // turn this off for the etx stuff
#define BRN_DSR_RREQ_TTL2            255
#define BRN_DSR_RREQ_DELAY1          500  // ms
#define BRN_DSR_RREQ_DELAY2          500  // ms
#define BRN_DSR_RREQ_BACKOFF_FACTOR  2
#define BRN_DSR_RREQ_MAX_DELAY       5000 // uh, reasonable?

#define BRN_DSR_RREQ_ISSUE_TIMER_INTERVAL 500 // how often to check if its time to issue a new request (ms)
//#define BRN_DSR_RREQ_ISSUE_TIMER_INTERVAL 10000 // how often to check if its time to issue a new request (ms)

    InitiatedReq(EtherAddress targ, IPAddress targ_ip, EtherAddress sarg, IPAddress sarg_ip) {
      _target = targ;
      _target_ip = targ_ip;
      _source = sarg;
      _source_ip = sarg_ip;
      _ttl = BRN_DSR_RREQ_TTL1;
      _times_issued = 1;
      _backoff_interval = BRN_DSR_RREQ_DELAY1;

      _time_last_issued = Timestamp::now().timeval();

      check();
    }
    InitiatedReq() {
      // need this for bighashmap::pair to work
    }
    void check() const {
      assert(_target);
      assert(_source);
      assert(_ttl > 0);
      //assert(_time_last_issued.tv_sec > 0);
 //robert
      //assert(_time_last_issued.tv_usec > 0);
      assert(_times_issued > 0);
      assert(_backoff_interval > 0);
    }
  };

  typedef HashMap<EtherAddress, InitiatedReq> InitiatedReqMap;
  typedef InitiatedReqMap::iterator InitReqIter;
  typedef HashMap<EtherAddress, Packet *> RequestsToForward;

 public:

  //---------------------------------------------------------------------------
  // fields
  //---------------------------------------------------------------------------
  int _debug;

  ForwardedReqMap _forwarded_rreq_map;
  // unique route request id
  uint16_t _rreq_id;
  bool _sendbuffer_check_routes;
  Timer _sendbuffer_timer;

  /* Route to be used for Testcases */
  RouteMap fixed_routes;


  //---------------------------------------------------------------------------
  // public methods
  //---------------------------------------------------------------------------

  RouteQuerier();
  ~RouteQuerier();

  const char *class_name() const { return "RouteQuerier"; }
  const char *port_count() const { return "1/2"; }
  const char *processing() const { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void push(int, Packet *);

  static void static_rreq_expire_hook(Timer *, void *);
  void rreq_expire_hook();

  static void static_rreq_issue_hook(Timer *, void *);
  void rreq_issue_hook();

  static void static_sendbuffer_timer_hook(Timer *, void *);
  void sendbuffer_timer_hook();

  static void static_blacklist_timer_hook(Timer *, void *);
  void blacklist_timer_hook();

  unsigned short route_metric(RouteQuerierRoute);
  int check_blacklist(EtherAddress);

  void set_blacklist(EtherAddress ether, int s);
  void stop_issuing_request(EtherAddress host);

  bool metric_preferable(unsigned short a, unsigned short b);

 private:

  //---------------------------------------------------------------------------
  // private fields
  //---------------------------------------------------------------------------
  NodeIdentity *_me;
  BrnLinkTable *_link_table;
  DSREncap *_dsr_encap;
  BRNEncap *_brn_encap;
  DSRDecap *_dsr_decap;
  BrnIappStationTracker  *_brn_iapp;

  SBMap _sendbuffer_map;
  InitiatedReqMap _initiated_rreq_map;

  Blacklist _blacklist;
  RequestsToForward _rreqs_to_foward;

  Timer _rreq_expire_timer;
  Timer _rreq_issue_timer;
  Timer _blacklist_timer;

  GenericMetric *_metric;
  bool _use_blacklist;

  //---------------------------------------------------------------------------
  // private methods
  //---------------------------------------------------------------------------

  void start_issuing_request(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip);

  void issue_rreq(EtherAddress dst, IPAddress dst_ip, EtherAddress src, IPAddress src_ip, unsigned int ttl);

  bool buffer_packet(Packet *p);

  static unsigned long diff_in_ms(timeval, timeval);

  unsigned char get_metric(EtherAddress);

  EtherAddress last_forwarder_eth(Packet *);

  void check();

 public:
  void flush_sendbuffer();
};

inline unsigned int hashcode(const ForwardedReqKey &f) {
  return ((unsigned int)( // XXX is this reasonable?
			 hashcode(f._src) ^
			 hashcode(f._target) ^
			 f._id));
}
//TODO: Remove next line
#define FRHASHCODE

CLICK_ENDDECLS
#endif
