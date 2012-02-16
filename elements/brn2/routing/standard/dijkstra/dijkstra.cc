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
 * Dijkstra.{cc,hh} -- Shortes Path
 * R. Sombrutzki (A. Zubow)
 *
 */
#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"

#include "dijkstra.hh"

CLICK_DECLS

Dijkstra::Dijkstra()
  : _node_identity(),
    _timer(this)
{
  RoutingAlgorithm::init();
  _min_link_metric_within_route = BRN_LT_DEFAULT_MIN_METRIC_IN_ROUTE;
}

Dijkstra::~Dijkstra()
{
}

int
Dijkstra::initialize (ErrorHandler *)
{
  BRN2Device *dev;

  //_timer.initialize(this);
  //_timer.schedule_now();

  return 0;
}

void
Dijkstra::run_timer(Timer*)
{
  //_timer.schedule_after_msec(5000);
}

void *
Dijkstra::cast(const char *n)
{
  if (strcmp(n, "Dijkstra") == 0)
    return (Dijkstra *) this;
  else
    return 0;
}

int
Dijkstra::configure (Vector<String> &conf, ErrorHandler *errh)
{
  int ret = cp_va_kparse(conf, this, errh,
        "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_identity,
        "LINKTABLE", cpkP+cpkM, cpElement, &_lt,
        "ROUTETABLE", cpkP+cpkM, cpElement, &_brn_routetable,
        "MIN_LINK_METRIC_IN_ROUTE", cpkP+cpkM, cpInteger, &_min_link_metric_within_route,
        "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd);

  return ret;
}

void
Dijkstra::take_state(Element *e, ErrorHandler *) {
  Dijkstra *q = (Dijkstra *)e->cast("LinkTable");
  if (!q) return;
}

void
Dijkstra::dijkstra(EtherAddress src, bool from_me)
{

  if (!_lt->_hosts.findp(src)) {
    return;
  }

  Timestamp start = Timestamp::now();

  typedef HashMap<EtherAddress, bool> EtherMap;
  EtherMap ether_addrs;

  for (HTIter iter = _lt->_hosts.begin(); iter.live(); iter++) {
    ether_addrs.insert(iter.value()._ether, true);
  }

  for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
    /* clear them all initially */
    BrnHostInfo *n = _lt->_hosts.findp(i.key());
    assert(n);
    n->clear(from_me);
  }
  BrnHostInfo *root_info = _lt->_hosts.findp(src);

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
    BrnHostInfo *current_min = _lt->_hosts.findp(current_min_ether);
    assert(current_min);
    if (from_me) {
      current_min->_marked_from_me = true;
    } else {
      current_min->_marked_to_me = true;
    }

    for (EtherMap::const_iterator i = ether_addrs.begin(); i.live(); i++) {
      BrnHostInfo *neighbor = _lt->_hosts.findp(i.key());
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
      BrnLinkInfo *lnfo = _lt->_links.findp(pair);
      if (NULL == lnfo || 0 == lnfo->_metric 
          || BRN_LT_INVALID_LINK_METRIC <= lnfo->_metric) {
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
      BrnHostInfo *nfo = _lt->_hosts.findp(i.key());
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

  dijkstra_time = Timestamp::now()-start;

  //if BRNDEBUG...
  //StringAccum sa;
  //sa << "dijstra took " << finish - start;
  //click_chatter("%s: %s\n", id().c_str(), sa.take_string().c_str());
}


enum {H_DIJKSTRA,
      H_DIJKSTRA_TIME};

static String 
LinkTable_read_param(Element *e, void *thunk)
{
  Dijkstra *td = (Dijkstra *)e;
    switch ((uintptr_t) thunk) {
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
LinkTable_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Dijkstra *f = (Dijkstra *)e;
  String s = cp_uncomment(in_s);
  switch((long)vparam) {
    case H_DIJKSTRA: {
      // run dijkstra for all associated clients
      EtherAddress m;
      if (!cp_ethernet_address(s, &m))
        return errh->error("dijkstra parameter must be etheraddress");

      f->dijkstra(m, true);
      break;
    }
  }
  return 0;
}

void
Dijkstra::add_handlers()
{
  RoutingAlgorithm::add_handlers();

  add_read_handler("dijkstra_time", LinkTable_read_param, (void *)H_DIJKSTRA_TIME);
  add_write_handler("dijkstra", LinkTable_write_param, (void *)H_DIJKSTRA);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Dijkstra)
