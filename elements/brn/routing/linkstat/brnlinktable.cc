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


/*
 * BrnLinkTable.{cc,hh} -- Routing Table in terms of links
 * A. Zubow
 *
 */
#include <click/config.h>
#include "elements/brn/routing/linkstat/brnlinktable.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <elements/wifi/path.hh>
#include <click/straccum.hh>

#include "elements/brn/nodeidentity.hh"

CLICK_DECLS

BrnLinkTable::BrnLinkTable() 
  : _debug(BrnLogger::DEFAULT),
//  _node_identity(),
  _timer(this),
  _sim_mode(false),
  _const_metric(0),
  _brn_routecache(NULL)
{
}

BrnLinkTable::~BrnLinkTable() 
{
  if (_debug) {
    Vector<EtherAddress> inodes;

    get_inodes(inodes);
    for (int i = 0; i < inodes.size(); i++) {
      BRN_INFO(" Inode: %s ", inodes[i].s().c_str());
    }
  }

  BRN_DEBUG(" * Linktable at shutdown: %s", print_links().c_str());
}

int
BrnLinkTable::initialize (ErrorHandler *) 
{
  _timer.initialize(this);
  _timer.schedule_now();
  return 0;
}

void
BrnLinkTable::run_timer(Timer*) 
{
  clear_stale();
  //fprintf(stderr, " * BrnLinkTable::run_timer()\n");
  // run dijkstra for all associated clients  
  /*
  if (_assoc_list) {
    for (int i = 0; i < _assoc_list->size(); i++) {
      dijkstra((*_assoc_list)[i], true);
      dijkstra((*_assoc_list)[i], false);
    }
  }
  */
  _timer.schedule_after_msec(5000);
}
void *
BrnLinkTable::cast(const char *n)
{
  if (strcmp(n, "BrnLinkTable") == 0)
    return (BrnLinkTable *) this;
  else
    return 0;
}
int
BrnLinkTable::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  int stale_period = 120;
  ret = cp_va_parse(conf, this, errh,
        cpOptional,
//        cpElement, "NodeIdentity", &_node_identity,
        cpElement, "BrnRouteCache", &_brn_routecache,
        cpKeywords,
        //"Ethernet", cpEtherAddress, "Ethernet address", &_ether,
        "STALE", cpUnsigned, "Stale info timeout", &stale_period,
        "SIMULATE", cpBool, "Simulation mode", &_sim_mode,
        "CONSTMETRIC", cpInteger, "CONSTMETRIC", &_const_metric,
        "MIN_LINK_METRIC_IN_ROUTE", cpInteger, "min link metric in route", &_brn_dsr_min_link_metric_within_route,
        cpEnd);

//  if (!_node_identity || !_node_identity->cast("NodeIdentity"))
//    return errh->error("NodeIdentity not specified");

  //if (!_ether) 
  //  return errh->error("Ethernet not specified");

  //if (!_brn_routecache)
  //  return errh->error("BrnRouteCache not specified");

  _stale_timeout.tv_sec = stale_period;
  _stale_timeout.tv_usec = 0;

  //_hosts.insert(_ether, BrnHostInfo(_ether));
  return ret;
}


void 
BrnLinkTable::take_state(Element *e, ErrorHandler *) {

  BrnLinkTable *q = (BrnLinkTable *)e->cast("LinkTable");
  if (!q) return;

  _hosts = q->_hosts;
  _links = q->_links;
  // run dijkstra for all associated clients  
  /*
  if (_assoc_list) {
    for (int i = 0; i < _assoc_list->size(); i++) {
      dijkstra((*_assoc_list)[i], true);
      dijkstra((*_assoc_list)[i], false);
    }
  }
  */
}

int
BrnLinkTable::static_update_link(const String &arg, Element *e,
			      void *, ErrorHandler *errh) 
{
  BrnLinkTable *n = (BrnLinkTable *) e;

  //if (n->_debug)
  //  click_chatter(" * BrnLinkTable::static_update_link()\n");

  Vector<String> args;
  EtherAddress from;
  EtherAddress to;
  uint32_t seq;
  uint32_t age;
  uint32_t metric;
  cp_spacevec(arg, args);

  if (args.size() != 5) {
    return errh->error("Must have three arguments: currently has %d: %s", args.size(), args[0].c_str());
  }

  if (!cp_ethernet_address(args[0], &from)) {
    return errh->error("Couldn't read EtherAddress out of from");
  }

  if (!cp_ethernet_address(args[1], &to)) {
    return errh->error("Couldn't read EtherAddress out of to");
  }
  if (!cp_unsigned(args[2], &metric)) {
    return errh->error("Couldn't read metric");
  }

  if (!cp_unsigned(args[3], &seq)) {
    return errh->error("Couldn't read seq");
  }

  if (!cp_unsigned(args[4], &age)) {
    return errh->error("Couldn't read age");
  }

  n->update_both_links(from, to, seq, age, metric);
  return 0;
}

void
BrnLinkTable::clear() 
{
  // TODO not clear permanent
  _hosts.clear();
  _links.clear();
}

bool 
BrnLinkTable::update_link(EtherAddress from, IPAddress from_ip, EtherAddress to, 
                        IPAddress to_ip, uint32_t seq, uint32_t age, uint32_t metric, bool permanent)
{
  // Flush the route cache
  //_brn_routecache->on_link_changed( from, to );
  BRN_DEBUG("update_link: %s %s %d", from.s().c_str(), to.s().c_str(), metric);

  if (!from || !to || !metric) {
    BRN_ERROR("ERROR: wrong arguments %s %s %d", from.s().c_str(), to.s().c_str(), metric);
    return false;
  }
  if (_stale_timeout.tv_sec < (int) age
      && false == permanent ) {
    BRN_WARN(" * not inserting to old link %s -> %s.", from.s().c_str(), to.s().c_str());
    return true;
  }

  /* make sure both the hosts exist */
  BrnHostInfo *nfrom = _hosts.findp(from);
  if (!nfrom) {
    _hosts.insert(from, BrnHostInfo(from, from_ip));
    nfrom = _hosts.findp(from);
  }
  BrnHostInfo *nto = _hosts.findp(to);
  if (!nto) {
    _hosts.insert(to, BrnHostInfo(to, to_ip));
    nto = _hosts.findp(to);
  }

  assert(nfrom);
  assert(nto);

  EthernetPair p = EthernetPair(from, to);
  BrnLinkInfo *lnfo = _links.findp(p);
  if (!lnfo) {
    _links.insert(p, BrnLinkInfo(from, to, seq, age, metric,permanent));
  } else {
    // AZu: new sequence number must be greater than the old one; otherwise no update will be taken
    uint32_t new_seq = seq;
    if (seq <= lnfo->_seq)
      new_seq = lnfo->_seq + 1;

    lnfo->update(new_seq, age, metric);
  }

  return true;
}

BrnLinkTable::BrnLink 
BrnLinkTable::random_link()
{
  int ndx = click_random() % _links.size();
  int current_ndx = 0;
  for (LTIter iter = _links.begin(); iter.live(); iter++, current_ndx++) {
    if (current_ndx == ndx) {
      BrnLinkInfo n = iter.value();
      return BrnLink(n._from, n._to, n._seq, n._metric);
    }
  }
  return BrnLink();

}

void 
BrnLinkTable::remove_node(const EtherAddress& node)
{
  if (!node)
    return;
    
  LTable::iterator iter = _links.begin();
  while (iter != _links.end())
  {
    BrnLinkInfo& nfo = iter.value();
    
    if (node == nfo._from || node == nfo._to) {
      BRN_DEBUG(" * link %s -> %s cleansed.", 
        nfo._from.s().c_str(), nfo._to.s().c_str() );
      
      LTable::iterator tmp = iter;
      iter++;
      _links.remove(tmp.key());
      continue;
    }
    
    iter++;
  }
}

void 
BrnLinkTable::get_hosts(Vector<EtherAddress> &v)
{
  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    BrnHostInfo n = iter.value();
    v.push_back(n._ether);
  }
}

uint32_t 
BrnLinkTable::get_host_metric_to_me(EtherAddress s)
{
  if (!s) {
    return 0;
  }
  BrnHostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric_to_me;
}

uint32_t 
BrnLinkTable::get_host_metric_from_me(EtherAddress s)
{
  if (!s) {
    return 0;
  }
  BrnHostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    return 0;
  }
  return nfo->_metric_from_me;
}

uint32_t 
BrnLinkTable::get_link_metric(EtherAddress from, EtherAddress to) 
{
  if (!from || !to) {
    return BRN_DSR_INVALID_ROUTE_METRIC;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return BRN_DSR_INVALID_ROUTE_METRIC;
  }
  EthernetPair p = EthernetPair(from, to);
  BrnLinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    if (_sim_mode) {
      return _const_metric; // TODO what is that? 
    } else {
      return BRN_DSR_INVALID_ROUTE_METRIC;
    }
  }
  return nfo->_metric;
}

uint32_t 
BrnLinkTable::get_link_seq(EtherAddress from, EtherAddress to) 
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  EthernetPair p = EthernetPair(from, to);
  BrnLinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  return nfo->_seq;
}

uint32_t 
BrnLinkTable::get_link_age(EtherAddress from, EtherAddress to) 
{
  if (!from || !to) {
    return 0;
  }
  if (_blacklist.findp(from) || _blacklist.findp(to)) {
    return 0;
  }
  EthernetPair p = EthernetPair(from, to);
  BrnLinkInfo *nfo = _links.findp(p);
  if (!nfo) {
    return 0;
  }
  struct timeval now;
  click_gettimeofday(&now);
  return nfo->age();
}

signed 
BrnLinkTable::get_route_metric(const Vector<EtherAddress> &route) 
{

  if ( route.size() == 0 ) return -1;
  if ( route.size() == 1 ) return 0;

  unsigned metric = 0;
  for (int i = 0; i < route.size() - 1; i++) {
    unsigned m = get_link_metric(route[i], route[i+1]);
/*    if (m == 0) {
      return 0;
*/
    metric += m;

    if ( m >= _brn_dsr_min_link_metric_within_route ) {  //BRN_DSR_MIN_LINK_METRIC_WITHIN_ROUTE
      BRN_DEBUG(" * metric %d is inferior as min_metric %d", m, _brn_dsr_min_link_metric_within_route);
      return -1;
    }
  }
  return metric;
}

String 
BrnLinkTable::ether_routes_to_string(const Vector< Vector<EtherAddress> > &routes)
{
  StringAccum sa;
  for (int x = 0; x < routes.size(); x++) {
    Vector <EtherAddress> r = routes[x];
    for (int i = 0; i < r.size(); i++) {
      if (i != 0) {
	sa << " ";
      }
      sa << r[i] << " ";
      if (i != r.size()-1) {
	sa << get_link_metric(r[i], r[i+1]);
      }
    }
    if (r.size() > 0) {
      sa << "\n";
    }
  }
  return sa.take_string();
}
bool
BrnLinkTable::valid_route(const Vector<EtherAddress> &route) 
{
  if (route.size() < 1) {
    return false;
  }
  /* ensure the metrics are all valid */
  unsigned metric = get_route_metric(route);
  if (metric  == 0 ||
      metric >= 777777){
    return false;
  }

  /* ensure that a node appears no more than once */
  for (int x = 0; x < route.size(); x++) {
    for (int y = x + 1; y < route.size(); y++) {
      if (route[x] == route[y]) {
	return false;
      }
    }
  }

  return true;
}

Vector<EtherAddress> 
BrnLinkTable::best_route(EtherAddress dst, bool from_me)
{
  Vector<EtherAddress> reverse_route;
  Vector<EtherAddress> route;
  if (!dst) {
    return route;
  }
  BrnHostInfo *nfo = _hosts.findp(dst);

  if (from_me) {
    while (nfo && nfo->_metric_from_me != 0) {
      reverse_route.push_back(nfo->_ether);
      nfo = _hosts.findp(nfo->_prev_from_me);
    }
    if (nfo && nfo->_metric_from_me == 0) {
    reverse_route.push_back(nfo->_ether);
    }
  } else {
    while (nfo && nfo->_metric_to_me != 0) {
      reverse_route.push_back(nfo->_ether);
      nfo = _hosts.findp(nfo->_prev_to_me);
    }
    if (nfo && nfo->_metric_to_me == 0) {
      reverse_route.push_back(nfo->_ether);
    }
  }

  // HACK BEGIN: remember last route and print differences
//  bool bEqual = (reverse_route.size() == last_route.size());
//  for( int i = 0; bEqual && i < reverse_route.size(); i++ )
//  {
//    bEqual = (reverse_route[i] == last_route[i]);
//  }
//  if( false == bEqual )
//  {
//    if (reverse_route.size() > 1) 
//    {
//    //  click_chatter(" * New route:\n");
//      for (int j=0; j < reverse_route.size(); j++) 
//      {
//    //    click_chatter(" - %d  %s\n", j, reverse_route[j].s().c_str());
//      }
//    }
//    last_route = reverse_route;
//  }
  // HACK END

//  if (from_me) {
//    /* why isn't there just push? */
//    for (int i=reverse_route.size() - 1; i >= 0; i--) {
//      route.push_back(reverse_route[i]);
//    }
//    return route;
//  }

  return reverse_route;
}

/*
String
BrnLinkTable::ether_routes_to_string(const Vector<Path> &routes)
{
  StringAccum sa;
  for (int x = 1; x < routes.size(); x++) {
    sa << path_to_string(routes[x]).c_str() << "\n";
  }
  return sa.take_string();
}
*/

String 
BrnLinkTable::print_links() 
{
  StringAccum sa;

  for (LTIter iter = _links.begin(); iter.live(); ++iter) {
    BrnLinkInfo n = iter.value();
    sa << "\t" << n._from.s().c_str() << " " << n._to.s().c_str();
    sa << " " << n._metric;
    sa << " " << n._seq << " " << n.age() << "\n";
  }
  return sa.take_string();
}

String 
BrnLinkTable::print_routes(bool from_me) 
{
  StringAccum sa;

  Vector<EtherAddress> ether_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++)
    ether_addrs.push_back(iter.key());

  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  for (int x = 0; x < ether_addrs.size(); x++) {
    EtherAddress ether = ether_addrs[x];
    Vector <EtherAddress> r = best_route(ether, from_me);
    if (valid_route(r)) {
      sa << ether.s().c_str() << " ";
      for (int i = 0; i < r.size(); i++) {
	sa << " " << r[i] << " ";
	if (i != r.size()-1) {
	  EthernetPair pair = EthernetPair(r[i], r[i+1]);
	  BrnLinkInfo *l = _links.findp(pair);
	  assert(l);
	  sa << l->_metric;
	  sa << " (" << l->_seq << "," << l->age() << ")";
	}
      }
      sa << "\n";
    }
  }
  return sa.take_string();
}

String 
BrnLinkTable::print_hosts() 
{
  StringAccum sa;
  Vector<EtherAddress> ether_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    ether_addrs.push_back(iter.key());
  }

  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  for (int x = 0; x < ether_addrs.size(); x++) {
    sa << ether_addrs[x] << "\n";
  }

  return sa.take_string();
}

void
BrnLinkTable::get_inodes(Vector<EtherAddress> &ether_addrs) 
{
  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    if (iter.value()._ip == IPAddress()) {
      ether_addrs.push_back(iter.key());
    } else {
      BRN_DEBUG(" * skip client node: %s (%s)", iter.key().s().c_str(), iter.value()._ip.s().c_str());
    }
  }
}

void 
BrnLinkTable::clear_stale() 
{
  LTable::iterator iter = _links.begin();
   
  while (iter != _links.end())
  {
    BrnLinkInfo& nfo = iter.value();
    if (!nfo._permanent 
        && (unsigned) _stale_timeout.tv_sec < nfo.age()) 
    {
      BRN_DEBUG(" * link %s -> %s timed out.", 
        nfo._from.s().c_str(), nfo._to.s().c_str() );
      
      LTable::iterator tmp = iter;
      iter++;
      _links.remove(tmp.key());
      continue;
    }
    
    iter++;
  }
}

void 
BrnLinkTable::get_neighbors(EtherAddress ether, Vector<EtherAddress> &neighbors) 
{
  typedef HashMap<EtherAddress, bool> EtherMap;
  EtherMap ether_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    ether_addrs.insert(iter.value()._ether, true);
  }

  for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
    BrnHostInfo *neighbor = _hosts.findp(i.key());
    assert(neighbor);
    if (ether != neighbor->_ether) {
      BrnLinkInfo *lnfo = _links.findp(EthernetPair(ether, neighbor->_ether));
      if (lnfo) {
	neighbors.push_back(neighbor->_ether);
      }
    }
  }
}

void
BrnLinkTable::dijkstra(EtherAddress src, bool from_me) 
{

  if (!_hosts.findp(src)) {
    return;
  }

  struct timeval start;
  struct timeval finish;

  click_gettimeofday(&start);

  typedef HashMap<EtherAddress, bool> EtherMap;
  EtherMap ether_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    ether_addrs.insert(iter.value()._ether, true);
  }

  for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
    /* clear them all initially */
    BrnHostInfo *n = _hosts.findp(i.key());
    assert(n);
    n->clear(from_me);
  }
  BrnHostInfo *root_info = _hosts.findp(src);

  assert(root_info);

  if (from_me) {
    root_info->_prev_from_me = root_info->_ether;
    root_info->_metric_from_me = 0;
  } else {
    root_info->_prev_to_me = root_info->_ether;
    root_info->_metric_to_me = 0;
  }

  EtherAddress current_min_ether = root_info->_ether;

  while (current_min_ether) {
    BrnHostInfo *current_min = _hosts.findp(current_min_ether);
    assert(current_min);
    if (from_me) {
      current_min->_marked_from_me = true;
    } else {
      current_min->_marked_to_me = true;
    }

    for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
      BrnHostInfo *neighbor = _hosts.findp(i.key());
      assert(neighbor);
      bool marked = neighbor->_marked_to_me;
      if (from_me) {
        marked = neighbor->_marked_from_me;
      }

      if (marked) {
        continue;
      }

      EthernetPair pair = EthernetPair(neighbor->_ether, current_min_ether);
      if (from_me) {
      	pair = EthernetPair(current_min_ether, neighbor->_ether);
      }
      BrnLinkInfo *lnfo = _links.findp(pair);
      if (NULL == lnfo || 0 == lnfo->_metric 
        || BRN_DSR_INVALID_ROUTE_METRIC <= lnfo->_metric) {
	     continue;
      }
      uint32_t neighbor_metric = neighbor->_metric_to_me;
      uint32_t current_metric = current_min->_metric_to_me;

      if (from_me) {
      	neighbor_metric = neighbor->_metric_from_me;
      	current_metric = current_min->_metric_from_me;
      }

      uint32_t adjusted_metric = current_metric + lnfo->_metric;
      if (!neighbor_metric || 
          adjusted_metric < neighbor_metric) {
      	if (from_me) {
      	  neighbor->_metric_from_me = adjusted_metric;
      	  neighbor->_prev_from_me = current_min_ether;
      	} else {
      	  neighbor->_metric_to_me = adjusted_metric;
      	  neighbor->_prev_to_me = current_min_ether;
      	}
      }
    }

    current_min_ether = EtherAddress();
    uint32_t  min_metric = ~0;
    for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
      BrnHostInfo *nfo = _hosts.findp(i.key());
      assert(nfo);
      uint32_t metric = nfo->_metric_to_me;
      bool marked = nfo->_marked_to_me;
      if (from_me) {
      	metric = nfo->_metric_from_me;
      	marked = nfo->_marked_from_me;
      }
      if (!marked && metric && 
    	  metric < min_metric) {
        current_min_ether = nfo->_ether;
        min_metric = metric;
      }
    }
  }

  click_gettimeofday(&finish);
  timersub(&finish, &start, &dijkstra_time);
  //StringAccum sa;
  //sa << "dijstra took " << finish - start;
  //click_chatter("%s: %s\n", id().c_str(), sa.take_string().c_str());
}


enum {H_BLACKLIST, 
      H_BLACKLIST_CLEAR, 
      H_BLACKLIST_ADD, 
      H_BLACKLIST_REMOVE,
      H_LINKS,
      H_ROUTES_FROM,
      H_ROUTES_TO,
      H_HOSTS,
      H_CLEAR,
      H_DIJKSTRA,
      H_DIJKSTRA_TIME,
      H_BEST_ROUTE,
      H_BESTROUTE_DIJKSTRA};

static String 
LinkTable_read_param(Element *e, void *thunk)
{
  BrnLinkTable *td = (BrnLinkTable *)e;
    switch ((uintptr_t) thunk) {
    case H_BLACKLIST: {
      StringAccum sa;
      typedef HashMap<EtherAddress, EtherAddress> EtherTable;
      typedef EtherTable::const_iterator EtherIter;

      for (EtherIter iter = td->_blacklist.begin(); iter.live(); iter++) {
	sa << iter.value() << " ";
      }
      return sa.take_string() + "\n";
    }
    case H_LINKS:  return td->print_links();
    case H_ROUTES_TO: return td->print_routes(false);
    case H_ROUTES_FROM: return td->print_routes(true);
    case H_HOSTS:  return td->print_hosts();
    case H_DIJKSTRA_TIME: {
      StringAccum sa;
      sa << td->dijkstra_time << "\n";
      return sa.take_string();
    }
    default:
      return String();
    }
}

static int 
LinkTable_write_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  BrnLinkTable *f = (BrnLinkTable *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
  case H_BLACKLIST_CLEAR: {
    f->_blacklist.clear();
    break;
  }
  case H_BLACKLIST_ADD: {
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("blacklist_add parameter must be etheraddress");
    f->_blacklist.insert(m, m);
    break;
  }
  case H_BLACKLIST_REMOVE: {
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("blacklist_add parameter must be etheraddress");
    f->_blacklist.remove(m);
    break;
  }
  case H_CLEAR: f->clear(); break;
  case H_DIJKSTRA: {
    // run dijkstra for all associated clients
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("dijkstra parameter must be etheraddress");
/*
    if (f->_assoc_list) {
      for (int i = 0; i < f->_assoc_list->size(); i++) {
        f->dijkstra((*(f->_assoc_list))[i], true);
        f->dijkstra((*(f->_assoc_list))[i], false);
      }
    } else*/ {
      f->dijkstra(m, true);
    }
    break;
  }
  case H_BEST_ROUTE: {
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("dijkstra parameter must be etheraddress");

    Vector<EtherAddress> route = f->best_route(m, true);

    for (int j=0; j<route.size(); j++) {
      click_chatter(" - %d  %s", j, route[j].s().c_str());
    }
    break;
  }
  case H_BESTROUTE_DIJKSTRA: {
    Vector<String> args;
    cp_spacevec(s, args);

    EtherAddress src, dst;
    if (args.size() != 2)
      return errh->error("dijkstra parameter must be etheraddress x etheraddress");
    if (!cp_ethernet_address(args[0], &src)) 
      return errh->error("dijkstra parameter must be etheraddress x etheraddress");
    if (!cp_ethernet_address(args[1], &dst)) 
      return errh->error("dijkstra parameter must be etheraddress x etheraddress");

    f->dijkstra(src, true);
    Vector<EtherAddress> route = f->best_route(dst, true);

    for (int j=0; j<route.size(); j++) {
      click_chatter(" - %d  %s", j, route[j].s().c_str());
    }
    break;
  }
  }
  return 0;
}

static String
read_debug_param(Element *e, void *)
{
  BrnLinkTable *lt = (BrnLinkTable *)e;
  return String(lt->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BrnLinkTable *lt = (BrnLinkTable *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  lt->_debug = debug;
  return 0;
}

void
BrnLinkTable::add_handlers()
{
  add_read_handler("routes", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_from", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_to", LinkTable_read_param, (void *)H_ROUTES_TO);
  add_read_handler("links", LinkTable_read_param, (void *)H_LINKS);
  add_read_handler("hosts", LinkTable_read_param, (void *)H_HOSTS);
  add_read_handler("blacklist", LinkTable_read_param, (void *)H_BLACKLIST);
  add_read_handler("dijkstra_time", LinkTable_read_param, (void *)H_DIJKSTRA_TIME);
  add_read_handler("debug", read_debug_param, 0);

  add_write_handler("clear", LinkTable_write_param, (void *)H_CLEAR);
  add_write_handler("blacklist_clear", LinkTable_write_param, (void *)H_BLACKLIST_CLEAR);
  add_write_handler("blacklist_add", LinkTable_write_param, (void *)H_BLACKLIST_ADD);
  add_write_handler("blacklist_remove", LinkTable_write_param, (void *)H_BLACKLIST_REMOVE);
  add_write_handler("dijkstra", LinkTable_write_param, (void *)H_DIJKSTRA);
  add_write_handler("best_route", LinkTable_write_param, (void *)H_BEST_ROUTE);
  add_write_handler("best_route_and_dijkstra", LinkTable_write_param, (void *)H_BESTROUTE_DIJKSTRA);
  add_write_handler("debug", write_debug_param, 0);

  add_write_handler("update_link", static_update_link, 0);
}

#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class HashMap<EtherAddress, EtherAddress>;
template class HashMap<EtherPair, BrnLinkTable::LinkInfo>;
template class HashMap<EtherAddress, BrnLinkTable::BrnHostInfo>;
#endif

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnLinkTable)
