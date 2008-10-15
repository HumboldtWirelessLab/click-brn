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

#ifndef CLICK_BRNLINKTABLE_HH
#define CLICK_BRNLINKTABLE_HH

#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include "common.hh"
#include "brnroutecache.hh"

CLICK_DECLS

/*
 * =c
 * BrnLinkTable(Ethernet Address, [STALE timeout])
 * =s BRN
 * Keeps a Link state database and calculates Weighted Shortest Path 
 * for other elements
 * =d
 * Runs dijkstra's algorithm occasionally.
 *
 */
/*
 * Represents a tupel pair in the link table.
 */
class EthernetPair {

 public:
  //
  //member
  //
  EtherAddress _to;
  EtherAddress _from;

  //
  //methods
  //
  EthernetPair() : _to(), _from() { }

  EthernetPair(EtherAddress from, EtherAddress to) {
      _to = to;
      _from = from;
  }

  bool contains(EtherAddress foo) {
    return ((foo == _to) || (foo == _from));
  }
  bool other(EtherAddress foo) { return ((_to == foo) ? _from : _to); }

  inline bool
  operator==(EthernetPair other)
  {
    return (other._to == _to && other._from == _from);
  }
};

inline unsigned
hashcode(EthernetPair p) 
{
return hashcode(p._to) + hashcode(p._from);
}


/*
 * Represents a link table storing {@link BrnLink} links.
 */
class BrnLinkTable: public Element {
 public:
  //
  //member
  //
  int _debug;
  //class NodeIdentity *_node_identity;
  Vector<EtherAddress> last_route;
 
  //
  //methods
  //
  /* generic click-mandated stuff*/
  BrnLinkTable();
  ~BrnLinkTable();
  
  void registerListOfAssociatedClients(Vector<EtherAddress> *assoc_list) {
    _assoc_list = assoc_list;
  }
  
  void add_handlers();
  const char* class_name() const { return "BrnLinkTable"; }
  int initialize(ErrorHandler *);
  void run_timer(Timer*);
  int configure(Vector<String> &conf, ErrorHandler *errh);
  void take_state(Element *, ErrorHandler *);
  void *cast(const char *n);
  /* read/write handlers */
  String print_routes(bool);
  String print_links();
  String print_hosts();
  void get_inodes(Vector<EtherAddress> &ether_addrs);


  static int static_update_link(const String &arg, Element *e,
				void *, ErrorHandler *errh);
  void clear();

  String ether_routes_to_string(const Vector< Vector<EtherAddress> > &routes);
  /* other public functions */

  bool update_link(EtherAddress from, EtherAddress to, 
                   uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false) {
    return update_link(from, IPAddress(), to, IPAddress(), seq, age, metric, permanent);
  }

  bool update_link(EtherAddress from, IPAddress from_ip, EtherAddress to, 
                   IPAddress to_ip, uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false);

  bool update_both_links(EtherAddress a, EtherAddress b, 
	uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false) {
   return update_both_links(a, IPAddress(), b, IPAddress(), seq,age, metric, permanent);
  }

  bool update_both_links(EtherAddress a, IPAddress a_ip, EtherAddress b, 
        IPAddress b_ip, uint32_t seq, uint32_t age, uint32_t metric,bool permanent=false) {
          if (update_link(a, a_ip, b, b_ip, seq,age, metric, permanent)) {
            return update_link(b, b_ip, a, a_ip, seq,age, metric, permanent);
    }
    return false;
  }
  
  /**
   * Removes all links containing the specified node
   * @param node @a [in] The node which links should be removed.
   */
  void remove_node(const EtherAddress& node);

  uint32_t get_link_metric(EtherAddress from, EtherAddress to);
  uint32_t get_link_seq(EtherAddress from, EtherAddress to);
  uint32_t get_link_age(EtherAddress from, EtherAddress to);
  bool valid_route(const Vector<EtherAddress> &route);
  //String ether_routes_to_string(const Vector<Path> &routes);
  signed get_route_metric(const Vector<EtherAddress> &route);
  void get_neighbors(EtherAddress ethernet, Vector<EtherAddress> &neighbors);
  void clear_stale();

  /**
   * @brief Query for a route between addrSrc and addrDst in cache
   * and link table.
   * @param addrSrc @a [in] The source of the route.
   * @param addrDst @a [in] The destination of the route.
   * @param route @a [in] Holds the route, if found.
   * @note Substitute for dijkstra and best_route
   */
  inline void query_route(
    /*[in]*/  const EtherAddress&   addrSrc,
    /*[in]*/  const EtherAddress&   addrDst,
    /*[out]*/ Vector<EtherAddress>& route );

  Vector< Vector<EtherAddress> > top_n_routes(EtherAddress dst, int n);
  uint32_t get_host_metric_to_me(EtherAddress s);
  uint32_t get_host_metric_from_me(EtherAddress s);
  void get_hosts(Vector<EtherAddress> &rv);

  class BrnLink {
  public:
    EtherAddress _from;
    EtherAddress _to;
    uint32_t _seq;
    uint32_t _metric;
    BrnLink() : _from(), _to(), _seq(0), _metric(0) { }
    BrnLink(EtherAddress from, EtherAddress to, uint32_t seq, uint32_t metric) {
      _from = from;
      _to = to;
      _seq = seq;
      _metric = metric;
    }
  };

  BrnLink random_link();

  typedef HashMap<EtherAddress, EtherAddress> EtherTable;
  typedef EtherTable::const_iterator EtherIter;

  //
  //member
  //
  EtherTable _blacklist;
  Vector<EtherAddress> *_assoc_list;

  struct timeval dijkstra_time;
  void dijkstra(EtherAddress src, bool);
  Vector<EtherAddress> best_route(EtherAddress dst, bool from_me);

  void get_stale_timeout(timeval& t) { 
    t.tv_sec = _stale_timeout.tv_sec; 
    t.tv_usec = _stale_timeout.tv_usec; 
  }

private:
  unsigned int _brn_dsr_min_link_metric_within_route;

  class BrnLinkInfo {
  public:
    EtherAddress _from;
    EtherAddress _to;
    unsigned _metric;
    uint32_t _seq;
    uint32_t _age;
    bool     _permanent;
    struct timeval _last_updated;
    BrnLinkInfo() { 
      _from = EtherAddress(); 
      _to = EtherAddress(); 
      _metric = 0; 
      _seq = 0;
      _age = 0;
      _last_updated.tv_sec = 0; 
    }

    BrnLinkInfo(EtherAddress from, EtherAddress to, 
      uint32_t seq, uint32_t age, unsigned metric, bool permanent=false) { 
      _from = from;
      _to = to;
      _metric = metric;
      _seq = seq;
      _age = age;
      _permanent = permanent;
      click_gettimeofday(&_last_updated);
    }

    BrnLinkInfo(const BrnLinkInfo &p) : 
      _from(p._from), _to(p._to), 
      _metric(p._metric), _seq(p._seq), 
      _age(p._age),
      _permanent(p._permanent),
      _last_updated(p._last_updated) 
    { }

    uint32_t age() {
      struct timeval now;
      click_gettimeofday(&now);
      return _age + (now.tv_sec - _last_updated.tv_sec);
    }
    void update(uint32_t seq, uint32_t age, unsigned metric) {
      if (seq <= _seq) {
	return;
      }
      _metric = metric; 
      _seq = seq;
      _age = age;
      click_gettimeofday(&_last_updated); 
    }

  };

  class BrnHostInfo {
  public:
    EtherAddress _ether;
    IPAddress _ip;
    uint32_t _metric_from_me;
    uint32_t _metric_to_me;

    EtherAddress _prev_from_me;
    EtherAddress _prev_to_me;

    bool _marked_from_me;
    bool _marked_to_me;

    BrnHostInfo(EtherAddress p, IPAddress ip) { 
      _ether = p;
      _ip = ip;
      _metric_from_me = 0; 
      _metric_to_me = 0; 
      _prev_from_me = EtherAddress(); 
      _prev_to_me = EtherAddress(); 
      _marked_from_me = false; 
      _marked_to_me = false; 
    }

    BrnHostInfo() : _ether(), _ip() { 
      //BrnHostInfo(EtherAddress(), IPAddress());
    }

    BrnHostInfo(const BrnHostInfo &p) : 
      _ether(p._ether), 
      _ip(p._ip),
      _metric_from_me(p._metric_from_me), 
      _metric_to_me(p._metric_to_me), 
      _prev_from_me(p._prev_from_me), 
      _prev_to_me(p._prev_to_me), 
      _marked_from_me(p._marked_from_me), 
      _marked_to_me(p._marked_to_me)
    { }

    void clear(bool from_me) { 
      if (from_me ) {
	_prev_from_me = EtherAddress(); 
	_metric_from_me = 0; 
	_marked_from_me = false;
      } else {
	_prev_to_me = EtherAddress(); 
	_metric_to_me = 0; 
	_marked_to_me = false;
      }
    }

  };

  typedef HashMap<EtherAddress, BrnHostInfo> HTable;
  typedef HTable::const_iterator HTIter;

  typedef HashMap<EthernetPair, BrnLinkInfo> LTable;
  typedef LTable::const_iterator LTIter;

  HTable _hosts;
  LTable _links;


  //EtherAddress _ether;
  struct timeval _stale_timeout;
  Timer _timer;
  bool _sim_mode;
  int _const_metric;
  
  BrnRouteCache *_brn_routecache;
};

inline void 
BrnLinkTable::query_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route )
{
  // First, search the route cache
  bool bCached = _brn_routecache->get_cached_route( addrSrc, 
                                                    addrDst, 
                                                    route );

  if( false == bCached )
  {
    // current node is not final destination of the packet,
    // so lookup route from dsr table and generate a dsr packet
    dijkstra(addrSrc, true);
  
    route = best_route(addrDst, true);
    
    // Cache the found route ...
    if( false == route.empty() )
    {
      _brn_routecache->insert_route( addrSrc, addrDst, route );
    }
  }
}

CLICK_ENDDECLS
#endif /* CLICK_BRNLINKTABLE_HH */
