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
 * .{cc,hh} --
 * R. Sombrutzki
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/routing/dsr/brn2_routeidcache.hh"

CLICK_DECLS

BrnRouteIdCache::BrnRouteIdCache()
{
  BRNElement::init();
}

BrnRouteIdCache::~BrnRouteIdCache()
{
}

int
BrnRouteIdCache::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BrnRouteIdCache::initialize(ErrorHandler *)
{
  routeIds.clear();
  return 0;
}

BrnRouteIdCache::RouteIdEntry*
BrnRouteIdCache::get_entry(EtherAddress *src, uint32_t id)
{
  BRN_DEBUG("Search ID-Entry: Src: %s ID: %d",src->unparse().c_str(), id);
  for ( int i = 0; i < routeIds.size(); i++ ) {
    if ( ( memcmp(routeIds[i]._src.data(), src->data(), 6 ) == 0 ) &&
         ( routeIds[i]._id == id ) )
       return &(routeIds[i]);
  }
  return NULL;
}

BrnRouteIdCache::RouteIdEntry*
BrnRouteIdCache::get_entry(EtherAddress *src, EtherAddress *dst)
{
  BRN_DEBUG("Search ID-Entry: Src: %s Dst: %s",src->unparse().c_str(), dst->unparse().c_str());

  for ( int i = 0; i < routeIds.size(); i++ ) {
    if ( ( memcmp(routeIds[i]._src.data(), src->data(), 6 ) == 0 ) &&
         ( memcmp(routeIds[i]._dst.data(), dst->data(), 6 ) == 0 ) )
      return &(routeIds[i]);
  }
  return NULL;
}

BrnRouteIdCache::RouteIdEntry*
BrnRouteIdCache::insert_entry(EtherAddress *src, EtherAddress *dst, EtherAddress *last_hop, EtherAddress *next_hop, uint32_t id)
{
  BrnRouteIdCache::RouteIdEntry *e = get_entry(src, id);

  BRN_DEBUG("Add ID-Entry");

  if ( e != NULL ) e->update_data(src, dst, last_hop, next_hop, id);
  else {
    routeIds.push_back(RouteIdEntry(EtherAddress(src->data()), EtherAddress(dst->data()), EtherAddress(last_hop->data()), EtherAddress(next_hop->data()), id));
    e = &routeIds[routeIds.size()-1];
  }

  return e;
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
BrnRouteIdCache::print_cache(void)
{
  StringAccum sa;

  sa << "RouteID-Cache:\n";
  sa << "Number\tID\tSRC\t\t\tDST\t\t\tLAST\t\t\tNEXT\n";
  for ( int i = 0; i < routeIds.size(); i++ ) {
    sa << (i+1) << "\t" << routeIds[i]._id;
    sa << "\t" << routeIds[i]._src.unparse();
    sa << "\t" << routeIds[i]._dst.unparse();
    sa << "\t" << routeIds[i]._last_hop.unparse();
    sa << "\t" << routeIds[i]._next_hop.unparse();
    sa << "\t" << routeIds[i]._used;
    sa << "\t" << routeIds[i]._insert_time.unparse() << "\n";
  }

  return sa.take_string();
}

static String
read_cache_param(Element *e, void *)
{
  BrnRouteIdCache *ridc = (BrnRouteIdCache *)e;
  return ridc->print_cache();
}

void
BrnRouteIdCache::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("cache", read_cache_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnRouteIdCache)
