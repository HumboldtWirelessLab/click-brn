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

#ifndef CLICK_BRN2LINKTABLE_HH
#define CLICK_BRN2LINKTABLE_HH

#include <click/glue.hh>
#include <click/timer.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/routecache/brn2routecache.hh"

#include "elements/brn2/brnelement.hh"

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


/* Sources of Linkupdates
 * Used to enable or disable linkupdates from a specific source
 */
#define LINKTABLE_UPDATESOURCE_LINKSTAT 0x0001
#define LINKTABLE_UPDATESOURCE_ROUTING  0x0002
#define LINKTABLE_UPDATESOURCE_LINKDIST 0x0004

/*
 * Represents a link table storing {@link BrnLink} links.
 */
class Brn2LinkTable: public BRNElement {
 public:
  //
  //member
  //
  bool _fix_linktable;
  //
  //methods
  //
  /* generic click-mandated stuff*/
  Brn2LinkTable();
  ~Brn2LinkTable();

  void add_handlers();
  const char* class_name() const { return "Brn2LinkTable"; }
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


  static int static_update_link(const String &arg, Element *e, void *, ErrorHandler *errh);
  void clear();

  String ether_routes_to_string(const Vector< Vector<EtherAddress> > &routes);
  /* other public functions */

  bool update_link(EtherAddress from, EtherAddress to,
                   uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false) {
    return update_link(from, IPAddress(), to, IPAddress(), seq, age, metric, permanent);
  }

  bool update_link(EtherAddress from, IPAddress from_ip, EtherAddress to,
                   IPAddress to_ip, uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false);

  bool update_both_links(EtherAddress a, EtherAddress b, uint32_t seq, uint32_t age,
                         uint32_t metric, bool permanent=false) {
    return update_both_links(a, IPAddress(), b, IPAddress(), seq, age, metric, permanent);
  }

  bool update_both_links(EtherAddress a, IPAddress a_ip, EtherAddress b, IPAddress b_ip,
                         uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false) {
    if (update_link(a, a_ip, b, b_ip, seq, age, metric, permanent)) {
      return update_link(b, b_ip, a, a_ip, seq, age, metric, permanent);
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

  int32_t get_route_metric(const Vector<EtherAddress> &route);
  void get_neighbors(EtherAddress ethernet, Vector<EtherAddress> &neighbors);
  void get_local_neighbors(Vector<EtherAddress> &neighbors);
  const EtherAddress *get_neighbor(EtherAddress ether);
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

  inline void update_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
            uint32_t metric);

  Vector< Vector<EtherAddress> > top_n_routes(EtherAddress dst, int n);
  uint32_t get_host_metric_to_me(EtherAddress s);
  uint32_t get_host_metric_from_me(EtherAddress s);
  void get_hosts(Vector<EtherAddress> &rv);

  typedef HashMap<EtherAddress, EtherAddress> EtherTable;
  typedef EtherTable::const_iterator EtherIter;

  //
  //member
  //
  EtherTable _blacklist;

  struct timeval dijkstra_time;
  void dijkstra(EtherAddress src, bool);
  Vector<EtherAddress> best_route(EtherAddress dst, bool from_me, uint32_t *metric);

  void get_stale_timeout(timeval& t) { 
    t.tv_sec = _stale_timeout.tv_sec; 
    t.tv_usec = _stale_timeout.tv_usec; 
  }

private:
  uint32_t _brn_dsr_min_link_metric_within_route;

  class BrnLinkInfo {
  public:
    EtherAddress _from;
    EtherAddress _to;
    uint32_t _metric;
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
      uint32_t seq, uint32_t age, uint32_t metric, bool permanent=false) {
      _from = from;
      _to = to;
      _metric = metric;
      _seq = seq;
      _age = age;
      _permanent = permanent;
      _last_updated = Timestamp::now().timeval();
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
      now = Timestamp::now().timeval();
      return _age + (now.tv_sec - _last_updated.tv_sec);
    }

    void update(uint32_t seq, uint32_t age, uint32_t metric) {
      if (seq <= _seq) {
        return;
      }
      _metric = metric;
      _seq = seq;
      _age = age;
      _last_updated = Timestamp::now().timeval();
    }

  };

 public:
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

    bool _is_associated;

    BrnHostInfo(EtherAddress p, IPAddress ip) {
      _ether = p;
      _ip = ip;
      _metric_from_me = 0;
      _metric_to_me = 0;
      _prev_from_me = EtherAddress();
      _prev_to_me = EtherAddress();
      _marked_from_me = false;
      _marked_to_me = false;
      _is_associated = false;
    }

    BrnHostInfo() : _ether(), _ip() {
    }

    BrnHostInfo(const BrnHostInfo &p) :
      _ether(p._ether),
      _ip(p._ip),
      _metric_from_me(p._metric_from_me),
      _metric_to_me(p._metric_to_me),
      _prev_from_me(p._prev_from_me),
      _prev_to_me(p._prev_to_me),
      _marked_from_me(p._marked_from_me),
      _marked_to_me(p._marked_to_me),
      _is_associated(p._is_associated)
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

  BRN2NodeIdentity *_node_identity;
  struct timeval _stale_timeout;
  Timer _timer;

  Brn2RouteCache *_brn_routecache;

 public:
  //Function is used for associated clients
  /* associated client are insert into the linktable and marked as associated*/
  bool associated_host(EtherAddress ea);  //TODO: check for better solution
  bool is_associated(EtherAddress ea);
};

inline void
Brn2LinkTable::query_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route )
{
  uint32_t metric;
  // First, search the route cache
  bool bCached = _brn_routecache->get_cached_route( addrSrc,
                                                    addrDst,
                                                    route,
                                                    &metric);

  if( false == bCached )
  {
    // current node is not final destination of the packet,
    // so lookup route from dsr table and generate a dsr packet
    dijkstra(addrSrc, true);

    route = best_route(addrDst, true, &metric);

    // Cache the found route ...
    if( false == route.empty() )
    {
      _brn_routecache->insert_route( addrSrc, addrDst, route, metric );
    }
  }
}

inline void
Brn2LinkTable::update_route(
  /*[in]*/  const EtherAddress&   addrSrc,
  /*[in]*/  const EtherAddress&   addrDst,
  /*[out]*/ Vector<EtherAddress>& route,
            uint32_t metric)
{

  Vector<EtherAddress> old_route;
  uint32_t old_metric;

  // First, search the route cache
  bool bCached = _brn_routecache->get_cached_route( addrSrc, addrDst, old_route, &old_metric );

  if( false == bCached )
  {
    // current node is not final destination of the packet,
    // so lookup route from dsr table and generate a dsr packet
    _brn_routecache->insert_route( addrSrc, addrDst, route, metric );
  } else {
    if ( old_metric > metric ) {
      _brn_routecache->insert_route( addrSrc, addrDst, route, metric );
    }
  }
}

CLICK_ENDDECLS
#endif /* CLICK_BRNLINKTABLE_HH */
