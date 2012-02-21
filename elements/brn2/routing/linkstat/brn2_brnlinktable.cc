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
    _timer(this)
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

    if ( dev->is_routable() ) add_node(*dev->getEtherAddress(), IPAddress(0));
  }

  return 0;
}

void
Brn2LinkTable::run_timer(Timer*)
{
  clear_stale();

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
        "STALE", cpkP+cpkM, cpUnsigned, &stale_period,
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

  //limit metric to BRN_LT_INVALID_LINK_METRIC
  if ( metric > BRN_LT_INVALID_LINK_METRIC ) metric = BRN_LT_INVALID_LINK_METRIC;

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
  BrnHostInfo *nfrom = add_node(from, from_ip);
  BrnHostInfo *nto = add_node(to, to_ip);

  assert(nfrom);
  assert(nto);

  EthernetPair p = EthernetPair(from, to);
  BrnLinkInfo *lnfo = _links.findp(p);
  if (!lnfo) {
    _links.insert(p, BrnLinkInfo(from, to, seq, age, metric, permanent));
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
  if (n) return n->_is_associated;

  return false;
}

void
Brn2LinkTable::remove_node(const EtherAddress& node)
{
  if (!node) return;

  BrnHostInfo *bhi = _hosts.findp(node);

  if ( bhi )
    for ( int i = 0; i < ltci.size(); i++ ) ltci[i]->remove_node(bhi);

  _hosts.remove(node);

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

BrnHostInfo *
Brn2LinkTable::add_node(const EtherAddress& node, IPAddress ip)
{
  if (!node) return NULL;
  BrnHostInfo *bhi = _hosts.findp(node);
  if ( bhi != NULL ) return bhi;
  _hosts.insert(node, BrnHostInfo( node, ip));
  bhi = _hosts.findp(node);
  for ( int i = 0; i < ltci.size(); i++ ) ltci[i]->add_node(bhi);
  return bhi;
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
Brn2LinkTable::get_host_metric_from_me(EtherAddress s)
{
  int best_metric = BRN_LT_INVALID_LINK_METRIC;

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
Brn2LinkTable::get_host_metric_to_me(EtherAddress s)
{
  return get_host_metric_from_me(s);
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
    return BRN_DSR_INVALID_ROUTE_METRIC;
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

//check for device used for this neighbour (ether)
const EtherAddress *
Brn2LinkTable::get_neighbor(EtherAddress ether)
{
  for ( int i = _node_identity->countDevices() - 1; i >= 0; i-- ) {
    BrnLinkInfo *lnfo = _links.findp(EthernetPair(ether, *(_node_identity->getDeviceByIndex(i)->getEtherAddress())));
    //TODO: check for device/etheraddr with best metric
    if (lnfo) {
      return _node_identity->getDeviceByIndex(i)->getEtherAddress();
    }
  }

  return NULL;
}

enum {H_BLACKLIST,
      H_BLACKLIST_CLEAR,
      H_BLACKLIST_ADD,
      H_BLACKLIST_REMOVE,
      H_LINKS,
      H_HOSTS,
      H_CLEAR,
      H_FIX_LINKTABLE };

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
  }

  sa << "</linktable>\n";
  return sa.take_string();
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
    case H_HOSTS:  return td->print_hosts();
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

  add_read_handler("links", LinkTable_read_param, (void *)H_LINKS);
  add_read_handler("hosts", LinkTable_read_param, (void *)H_HOSTS);
  add_read_handler("blacklist", LinkTable_read_param, (void *)H_BLACKLIST);
  add_read_handler("fix_linktable", LinkTable_read_param, (void *)H_FIX_LINKTABLE);

  add_write_handler("clear", LinkTable_write_param, (void *)H_CLEAR);
  add_write_handler("blacklist_clear", LinkTable_write_param, (void *)H_BLACKLIST_CLEAR);
  add_write_handler("blacklist_add", LinkTable_write_param, (void *)H_BLACKLIST_ADD);
  add_write_handler("blacklist_remove", LinkTable_write_param, (void *)H_BLACKLIST_REMOVE);
  add_write_handler("fix_linktable", LinkTable_write_param, (void *)H_FIX_LINKTABLE);

  add_write_handler("update_link", static_update_link, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2LinkTable)
