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
 * Brn2LinkTable.{cc,hh} -- Routing Table in terms of links
 * A. Zubow
 *
 */
#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"

#include "elements/brn2/routing/dsr/brn2_dsrprotocol.hh"

CLICK_DECLS

Brn2LinkTable::Brn2LinkTable()
  : _fix_linktable(false),
  _node_identity(),
  _timer(this),
  _sim_mode(false),
  _const_metric(0),
  _brn_routecache(NULL)
{
  BRNElement::init();
}

Brn2LinkTable::~Brn2LinkTable()
{
  if (_debug) {
    Vector<EtherAddress> inodes;

    get_inodes(inodes);
    for (int i = 0; i < inodes.size(); i++) {
      BRN_INFO(" Inode: %s ", inodes[i].unparse().c_str());
    }
  }

  BRN_DEBUG(" * Linktable at shutdown: %s", print_links().c_str());

  _links.clear();
  _hosts.clear();
  last_route.clear();
  _blacklist.clear();
}

int
Brn2LinkTable::initialize (ErrorHandler *)
{
  BRN2Device *dev;

  _timer.initialize(this);
  _timer.schedule_now();

  for ( int d = 0; d < _node_identity->countDevices(); d++ ) {
    dev = _node_identity->getDeviceByIndex(d);

    if ( dev->is_routable() )
      _hosts.insert(EtherAddress(dev->getEtherAddress()->data()), BrnHostInfo(EtherAddress(dev->getEtherAddress()->data()), IPAddress(0)));
  }

  return 0;
}

void
Brn2LinkTable::run_timer(Timer*)
{
  clear_stale();
  //fprintf(stderr, " * Brn2LinkTable::run_timer()\n");

  _timer.schedule_after_msec(5000);
}
void *
Brn2LinkTable::cast(const char *n)
{
  if (strcmp(n, "Brn2LinkTable") == 0)
    return (Brn2LinkTable *) this;
  else
    return 0;
}
int
Brn2LinkTable::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  int stale_period = 120;
  ret = cp_va_kparse(conf, this, errh,
        "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_identity,
        "ROUTECACHE", cpkP+cpkM, cpElement, &_brn_routecache,
        "STALE", cpkP+cpkM, cpUnsigned, &stale_period,
        "SIMULATE", cpkP+cpkM, cpBool, &_sim_mode,
        "CONSTMETRIC", cpkP+cpkM, cpInteger, &_const_metric,
        "MIN_LINK_METRIC_IN_ROUTE", cpkP+cpkM, cpInteger, &_brn_dsr_min_link_metric_within_route,
        "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd);

  _stale_timeout.tv_sec = stale_period;
  _stale_timeout.tv_usec = 0;

  return ret;
}


void 
Brn2LinkTable::take_state(Element *e, ErrorHandler *) {

  Brn2LinkTable *q = (Brn2LinkTable *)e->cast("LinkTable");
  if (!q) return;

  _hosts = q->_hosts;
  _links = q->_links;

}

int
Brn2LinkTable::static_update_link(const String &arg, Element *e, void *, ErrorHandler *errh)
{
  Brn2LinkTable *n = (Brn2LinkTable *) e;

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
Brn2LinkTable::clear()
{
  // TODO not clear permanent
  _hosts.clear();
  _links.clear();
}

bool 
Brn2LinkTable::update_link(EtherAddress from, IPAddress from_ip, EtherAddress to,
                        IPAddress to_ip, uint32_t seq, uint32_t age, uint32_t metric, bool permanent)
{
  /* Don't update the linktable if it should be fix (e.g. for measurement and debug) */
  /* TODO: check age and permant-flag of links */
  if ( _fix_linktable ) return true;

  // Flush the route cache
  //_brn_routecache->on_link_changed( from, to );
  BRN_DEBUG("update_link: %s %s %d", from.unparse().c_str(), to.unparse().c_str(), metric);

  if (!from || !to || !metric) {
    BRN_ERROR("ERROR: wrong arguments %s %s %d", from.unparse().c_str(), to.unparse().c_str(), metric);
    return false;
  }

  if (_stale_timeout.tv_sec < (int) age
      && false == permanent ) {
    BRN_WARN(" * not inserting to old link %s -> %s.", from.unparse().c_str(), to.unparse().c_str());
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

bool
Brn2LinkTable::associated_host(EtherAddress ea)
{
  BrnHostInfo *n = _hosts.findp(ea);
  if (n) n->_is_associated = true;

  return true;
}

bool
Brn2LinkTable::is_associated(EtherAddress ea)
{
  BrnHostInfo *n = _hosts.findp(ea);
  if (n) return  n->_is_associated;

  return false;
}

void
Brn2LinkTable::remove_node(const EtherAddress& node)
{
  if (!node)
    return;

  LTable::iterator iter = _links.begin();
  while (iter != _links.end())
  {
    BrnLinkInfo& nfo = iter.value();

    if (node == nfo._from || node == nfo._to) {
      BRN_DEBUG(" * link %s -> %s cleansed.",
        nfo._from.unparse().c_str(), nfo._to.unparse().c_str() );

      LTable::iterator tmp = iter;
      iter++;
      _links.remove(tmp.key());
      continue;
    }

    iter++;
  }
}

void 
Brn2LinkTable::get_hosts(Vector<EtherAddress> &v)
{
  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    BrnHostInfo n = iter.value();
    v.push_back(n._ether);
  }
}

uint32_t 
Brn2LinkTable::get_host_metric_to_me(EtherAddress s)
{
  BRN_DEBUG("Search host %s in Hosttable",s.unparse().c_str());
  if (!s) {
    return 0;
  }
  BrnHostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    BRN_DEBUG("Couldn't find host %s in Hosttable",s.unparse().c_str());
    return 0;
  }
  BRN_DEBUG("Find host %s in Hosttable",s.unparse().c_str());
  return nfo->_metric_to_me;
}

/*
uint32_t 
Brn2LinkTable::get_host_metric_from_me(EtherAddress s)
{
  if (!s) {
    return 0;
  }
  BrnHostInfo *nfo = _hosts.findp(s);
  if (!nfo) {
    click_chatter("Neighbour not found");
    return BRN_DSR_INVALID_ROUTE_METRIC; //TODO: return value was 0 before. Check what is correct
  }
  return nfo->_metric_from_me;
}
*/
uint32_t 
Brn2LinkTable::get_host_metric_from_me(EtherAddress s)
{
  int best_metric = BRN_DSR_INVALID_ROUTE_METRIC;

  for ( int d = 0; d < _node_identity->countDevices(); d++ ) {
    int c_metric = get_link_metric(s, *(_node_identity->getDeviceByIndex(d)->getEtherAddress()));
    /* TODO: use "is_better"-funtion of metric */
    if ( c_metric < best_metric ) {
      best_metric = c_metric;
    }
  }

  return best_metric;
}


uint32_t 
Brn2LinkTable::get_link_metric(EtherAddress from, EtherAddress to)
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
Brn2LinkTable::get_link_seq(EtherAddress from, EtherAddress to)
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
Brn2LinkTable::get_link_age(EtherAddress from, EtherAddress to)
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
  now = Timestamp::now().timeval();
  return nfo->age();
}

signed
Brn2LinkTable::get_route_metric(const Vector<EtherAddress> &route)
{

  if ( route.size() == 0 ) return -1;
  if ( route.size() == 1 ) return 0;

  unsigned metric = 0;
  for (int i = 0; i < route.size() - 1; i++) {
    EtherAddress src = route[i];
    EtherAddress dst = route[i+1];
    unsigned m = get_link_metric(src, dst);

    metric += m;

    if ( m >= _brn_dsr_min_link_metric_within_route ) {
      BRN_DEBUG(" * metric %d is inferior as min_metric %d", m, _brn_dsr_min_link_metric_within_route);
      return -1;
    }
  }
  return metric;
}

String 
Brn2LinkTable::ether_routes_to_string(const Vector< Vector<EtherAddress> > &routes)
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
Brn2LinkTable::valid_route(const Vector<EtherAddress> &route)
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
Brn2LinkTable::best_route(EtherAddress dst, bool from_me, uint32_t *metric)
{
  Vector<EtherAddress> reverse_route;
  Vector<EtherAddress> route;

  if (!dst) {
    metric = 0;
    return route;
  }
  BrnHostInfo *nfo = _hosts.findp(dst);

  if (from_me) {
    while (nfo && nfo->_metric_from_me != 0) {
      reverse_route.push_back(nfo->_ether);
      *metric = nfo->_metric_from_me;
      nfo = _hosts.findp(nfo->_prev_from_me);
    }
    if (nfo && nfo->_metric_from_me == 0) {
    reverse_route.push_back(nfo->_ether);
    }
  } else {
    while (nfo && nfo->_metric_to_me != 0) {
      *metric = nfo->_metric_to_me;
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
//    //    click_chatter(" - %d  %s\n", j, reverse_route[j].unparse().c_str());
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
Brn2LinkTable::ether_routes_to_string(const Vector<Path> &routes)
{
  StringAccum sa;
  for (int x = 1; x < routes.size(); x++) {
    sa << path_to_string(routes[x]).c_str() << "\n";
  }
  return sa.take_string();
}
*/

String 
Brn2LinkTable::print_links()
{
  StringAccum sa;
  sa << "<linktable id=\"";
  sa << _node_identity->getMasterAddress()->unparse().c_str();
  sa << "\">\n";
	
  for (LTIter iter = _links.begin(); iter.live(); ++iter) {
    BrnLinkInfo n = iter.value();
    sa << "\t<link from=\"" << n._from.unparse().c_str() << "\" to=\"" << n._to.unparse().c_str() << "\"";
    sa << " metric=\"" << n._metric << "\"";
    sa << " seq=\"" << n._seq << "\" age=\"" << n.age() << "\" />\n";
    
    //the following lines printing out linktable raw, not in XML
    //sa << "\t" << n._from.unparse().c_str() << " " << n._to.unparse().c_str();
    //sa << " " << n._metric;
    //sa << " " << n._seq << " " << n.age() << "\n";
  }
  
  sa << "</linktable>\n";
  return sa.take_string();
}

String 
Brn2LinkTable::print_routes(bool from_me)
{
  StringAccum sa;

  Vector<EtherAddress> ether_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++)
    ether_addrs.push_back(iter.key());

  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);
  
  sa << "<routetable id=\"";
  sa << _node_identity->getMasterAddress()->unparse().c_str();
  sa << "\">\n";
  
  for (int x = 0; x < ether_addrs.size(); x++) {
    EtherAddress ether = ether_addrs[x];
    uint32_t metric_trash;
    Vector <EtherAddress> r = best_route(ether, from_me, &metric_trash);
    if (valid_route(r)) {
      sa << "\t<route from=\"" << r[0] << "\" to=\"" << r[r.size()-1] << "\">\n";
      //sa << ether.unparse().c_str() << " ";
      
		for (int i = 0; i < r.size()-1; i++) {
			EthernetPair pair = EthernetPair(r[i], r[i+1]);
			BrnLinkInfo *l = _links.findp(pair);
			sa << "\t\t<link from=\"" << r[i] << "\" to=\"" << r[i+1] << "\" ";
			sa << "metric=\"" << l->_metric << "\" ";
			sa << "seq=\"" << l->_seq << "\" age=\"" << l->age() << "\" />\n";
		}
		sa << "\t</route>\n";

//      for (int i = 0; i < r.size(); i++) {		  
//		  sa << " " << r[i] << " ";
//		  if (i != r.size()-1) {
//			  EthernetPair pair = EthernetPair(r[i], r[i+1]);
//			  BrnLinkInfo *l = _links.findp(pair);
//			  if (l) {
//				  sa << l->_metric;
//				  sa << " (" << l->_seq << "," << l->age() << ")";
//			  } else {
//				  sa << "(warning, lnkinfo not found!!!)";
//			  }
//		  }
//  }
//	  sa << "\n";
    }
  }
  
  sa << "</routetable>\n";
  
  return sa.take_string();
}

String 
Brn2LinkTable::print_hosts()
{
  StringAccum sa;
  Vector<EtherAddress> ether_addrs;

  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    ether_addrs.push_back(iter.key());
  }

  click_qsort(ether_addrs.begin(), ether_addrs.size(), sizeof(EtherAddress), etheraddr_sorter);

  for (int x = 0; x < ether_addrs.size(); x++) {
    BrnHostInfo *nfo = _hosts.findp(ether_addrs[x]);
    sa << ether_addrs[x] << " from_me: " << nfo->_metric_from_me << " to_me: " << nfo->_metric_to_me << " assosiated: " << nfo->_is_associated << "\n";
  }

  return sa.take_string();
}

void
Brn2LinkTable::get_inodes(Vector<EtherAddress> &ether_addrs)
{
  for (HTIter iter = _hosts.begin(); iter.live(); iter++) {
    if (iter.value()._ip == IPAddress()) {
      ether_addrs.push_back(iter.key());
    } else {
      BRN_DEBUG(" * skip client node: %s (%s)", iter.key().unparse().c_str(), iter.value()._ip.unparse().c_str());
    }
  }
}

void 
Brn2LinkTable::clear_stale()
{
  LTable::iterator iter = _links.begin();

  while (iter != _links.end())
  {
    BrnLinkInfo& nfo = iter.value();
    if (!nfo._permanent 
        && (unsigned) _stale_timeout.tv_sec < nfo.age())
    {
      BRN_DEBUG(" * link %s -> %s timed out.", 
        nfo._from.unparse().c_str(), nfo._to.unparse().c_str() );

      LTable::iterator tmp = iter;
      iter++;
      _links.remove(tmp.key());
      continue;
    }

    iter++;
  }
}

void 
Brn2LinkTable::get_neighbors(EtherAddress ether, Vector<EtherAddress> &neighbors)
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

/** TODO:
  Check every device for neighbors and not only the master-device
*/
void
Brn2LinkTable::get_local_neighbors(Vector<EtherAddress> &neighbors) {
  const EtherAddress *me = _node_identity->getMasterAddress();
  get_neighbors(*me,neighbors);
}

//TODO: Short this function
EtherAddress * 
Brn2LinkTable::get_neighbor(EtherAddress ether)
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
        return &(neighbor->_ether);
      }
    }
  }

  return NULL;
}

void
Brn2LinkTable::dijkstra(EtherAddress src, bool from_me)
{

  if (!_hosts.findp(src)) {
    return;
  }

  struct timeval start;
  struct timeval finish;

  start = Timestamp::now().timeval();

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

  finish = Timestamp::now().timeval();
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
      H_BESTROUTE_DIJKSTRA,
      H_FIX_LINKTABLE};

static String 
LinkTable_read_param(Element *e, void *thunk)
{
  Brn2LinkTable *td = (Brn2LinkTable *)e;
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
    case H_FIX_LINKTABLE: {
      return "<fixlinktable value=\"" + String(td->_fix_linktable) + "\" />\n";
    }
    default:
      return String();
    }
}

static int 
LinkTable_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Brn2LinkTable *f = (Brn2LinkTable *)e;
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

    f->dijkstra(m, true);
    break;
  }
  case H_BEST_ROUTE: {
    EtherAddress m;
    if (!cp_ethernet_address(s, &m)) 
      return errh->error("dijkstra parameter must be etheraddress");

    uint32_t metric_trash;
    Vector<EtherAddress> route = f->best_route(m, true, &metric_trash);

    for (int j=0; j<route.size(); j++) {
      click_chatter(" - %d  %s", j, route[j].unparse().c_str());
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
    uint32_t metric_trash;
    Vector<EtherAddress> route = f->best_route(dst, true, &metric_trash);

    for (int j=0; j<route.size(); j++) {
      click_chatter(" - %d  %s", j, route[j].unparse().c_str());
    }
    break;
  }
  case H_FIX_LINKTABLE: {
    bool fix_linktable;
    if (!cp_bool(s, &fix_linktable)) {
      return errh->error("fix_linktable is a boolean");
    } else {
      f->_fix_linktable = fix_linktable;
    }

    break;
  }
  }
  return 0;
}

void
Brn2LinkTable::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("routes", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_from", LinkTable_read_param, (void *)H_ROUTES_FROM);
  add_read_handler("routes_to", LinkTable_read_param, (void *)H_ROUTES_TO);
  add_read_handler("links", LinkTable_read_param, (void *)H_LINKS);
  add_read_handler("hosts", LinkTable_read_param, (void *)H_HOSTS);
  add_read_handler("blacklist", LinkTable_read_param, (void *)H_BLACKLIST);
  add_read_handler("dijkstra_time", LinkTable_read_param, (void *)H_DIJKSTRA_TIME);
  add_read_handler("fix_linktable", LinkTable_read_param, (void *)H_FIX_LINKTABLE);

  add_write_handler("clear", LinkTable_write_param, (void *)H_CLEAR);
  add_write_handler("blacklist_clear", LinkTable_write_param, (void *)H_BLACKLIST_CLEAR);
  add_write_handler("blacklist_add", LinkTable_write_param, (void *)H_BLACKLIST_ADD);
  add_write_handler("blacklist_remove", LinkTable_write_param, (void *)H_BLACKLIST_REMOVE);
  add_write_handler("dijkstra", LinkTable_write_param, (void *)H_DIJKSTRA);
  add_write_handler("best_route", LinkTable_write_param, (void *)H_BEST_ROUTE);
  add_write_handler("best_route_and_dijkstra", LinkTable_write_param, (void *)H_BESTROUTE_DIJKSTRA);
  add_write_handler("fix_linktable", LinkTable_write_param, (void *)H_FIX_LINKTABLE);

  add_write_handler("update_link", static_update_link, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2LinkTable)
