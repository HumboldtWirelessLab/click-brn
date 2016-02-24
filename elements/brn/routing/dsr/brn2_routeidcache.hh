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

#ifndef BRNDSRROUTEIDCACHEELEMENT_HH
#define BRNDSRROUTEIDCACHEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class BrnRouteIdCache : public BRNElement {

 public:
  class RouteIdEntry {
   public:

    EtherAddress _src;
    EtherAddress _dst;
    EtherAddress _in_addr;
    EtherAddress _out_addr;
    EtherAddress _last_hop;
    EtherAddress _next_hop;

    uint32_t _id;

    uint32_t _used;

    Timestamp _insert_time;

    RouteIdEntry(EtherAddress src, EtherAddress dst, EtherAddress last_hop, EtherAddress next_hop, uint32_t id): _src(src), _dst(dst), _last_hop(last_hop),
                                                                                                                 _next_hop(next_hop), _id(id), _used(0), _insert_time(Timestamp::now())
    { }

    void update_data(EtherAddress *src, EtherAddress *dst, EtherAddress *last_hop, EtherAddress *next_hop, uint32_t id) {
      memcpy(_src.data(),src->data(),6);
      memcpy(_dst.data(),dst->data(),6);
      memcpy(_last_hop.data(),last_hop->data(),6);
      memcpy(_next_hop.data(),next_hop->data(),6);

      _id = id;

      _insert_time = Timestamp::now();
    }


    void update() {
      _insert_time = Timestamp::now();
      _used++;
    }

  };

 public:
  //
  //methods
  //
  BrnRouteIdCache();
  ~BrnRouteIdCache();

  const char *class_name() const	{ return "BrnRouteIdCache"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();
  String print_cache(void);

  Vector<RouteIdEntry> routeIds;

  BrnRouteIdCache::RouteIdEntry* get_entry(EtherAddress *src, uint32_t id);
  BrnRouteIdCache::RouteIdEntry* get_entry(EtherAddress *src, EtherAddress *dst);
  BrnRouteIdCache::RouteIdEntry* insert_entry(EtherAddress *src, EtherAddress *dst, EtherAddress *last_hop, EtherAddress *next_hop, uint32_t id);

 public:

};

CLICK_ENDDECLS
#endif
