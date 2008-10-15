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

#ifndef CLICK_BRNAVGCNT_HH
#define CLICK_BRNAVGCNT_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/atomic.hh>
#include <click/timer.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include "nodeidentity.hh"

CLICK_DECLS

/*
 * =c
 * BrnAvgCnt([IGNORE])
 * =s measurement
 * measures historical packet count and rate
 * =d
 */

class BrnAvgCnt : public Element {

 public:
  typedef HashMap<EtherAddress, uint32_t> StatMap;
  typedef StatMap::iterator StatMapIter;

  NodeIdentity *_me;
  StatMap _stat_map;
  atomic_uint32_t _last; // last update

 public:

  BrnAvgCnt();
  ~BrnAvgCnt();

  const char *class_name() const		{ return "BrnAvgCnt"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);

  uint32_t last() const			        { return _last; }
  void update_last()  				{ _last = click_jiffies(); }
  void reset();

  int initialize(ErrorHandler *);
  void add_handlers();

  Packet *simple_action(Packet *);
  uint32_t get_traffic_count(EtherAddress &dst, int &delta);
};

CLICK_ENDDECLS
#endif
