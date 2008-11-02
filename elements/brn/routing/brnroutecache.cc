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

/**
 * @file brnroutecache.cc
 * @brief Implementation of the BrnRouteCache class.
 */
 
#include <click/config.h>
#include "elements/brn/common.hh"

#include <click/confparse.hh>
#include "brnroutecache.hh"
CLICK_DECLS

////////////////////////////////////////////////////////////////////////

BrnRouteCache::BrnRouteCache() :
  _debug(BrnLogger::DEFAULT),
  m_bActive( false ),
  m_tRouteAging( on_routeaging_expired, this ),
  m_iInitialTTL( 20 ),
  m_iDropProb( 50 )
{
  m_tvLifetimeSlice.tv_sec  = 1;
  m_tvLifetimeSlice.tv_usec = 0;
}

////////////////////////////////////////////////////////////////////////

BrnRouteCache::~BrnRouteCache()
{
}

////////////////////////////////////////////////////////////////////////
int 
BrnRouteCache::initialize(ErrorHandler *)
{
  m_tRouteAging.initialize(this);

  // Reschedule the timer
  if( m_tvLifetimeSlice.tv_sec < 1000000 )
  {
    m_tRouteAging.schedule_after( m_tvLifetimeSlice );
  }
  
  return( 0 );
}

////////////////////////////////////////////////////////////////////////
int 
BrnRouteCache::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int ret;
  uint32_t ullSlice = m_tvLifetimeSlice.tv_sec * 1000000 +
                      m_tvLifetimeSlice.tv_usec;
  
  ret = cp_va_kparse(conf, this, errh,
    "DEBUG", cpkN, cpInteger, /*"Debug indicator",*/ &_debug,
    "ACTIVE", cpkN, cpBool, /*"Active indicator",*/ &m_bActive,
    "DROP", cpkN, cpInteger, /*"Route flush probability",*/ &m_iDropProb,
    "TTL", cpkN, cpInteger, /*"Initial route ttl in slices",*/ &m_iInitialTTL,
    "SLICE", cpkN, cpUnsigned, /*"Lifetime slice in us",*/ &ullSlice,
    cpEnd);
  
  if( 0 != ullSlice )
  {
    m_tvLifetimeSlice.tv_sec  = ullSlice / 1000000;
    m_tvLifetimeSlice.tv_usec = ullSlice % 1000000;
  }
  else
  {
    m_tvLifetimeSlice.tv_sec  = 1000000;
    m_tvLifetimeSlice.tv_usec = 0;
  }
  
  return ret;
}

////////////////////////////////////////////////////////////////////////
bool 
BrnRouteCache::get_cached_route( 
  /*[in]*/  const AddressType&  addrSrc,
  /*[in]*/  const AddressType&  addrDst,
  /*[out]*/ RouteType&          route )
{
  if( false == m_bActive )
    return( false );

  EntryType* pEntry;
  route.clear();
  
  BRN_DEBUG("wuery for route %s -> %s.",
    addrSrc.unparse().c_str(), addrDst.unparse().c_str());
  
  // Find the corresponding entry
  pEntry = m_mapRoutes.findp( AddressPairType(addrSrc,addrDst) );
  if( NULL != pEntry )
  {
    int32_t iRand = (0 == m_iDropProb) ? 1 : click_random() % m_iDropProb;
    
    // Return the cached entry if the ttl is greater null and the 
    // random value is not equal null.
    if( 0 < pEntry->m_iTTL 
      && 0 != iRand )
    {
      BRN_DEBUG("using cached route.");

      route = pEntry->m_route;
      return( true );
    }

    BRN_DEBUG("random route drop.");
    
    // Delete the entry and fall through to return...
    pEntry = NULL;
    remove_route( addrSrc, addrDst );
  }
  
  BRN_DEBUG("no cached route found.");
  return( false );
}

////////////////////////////////////////////////////////////////////////
void 
BrnRouteCache::insert_route( 
  /*[in]*/ const AddressType& addrSrc,
  /*[in]*/ const AddressType& addrDst,
  /*[in]*/ const RouteType&   route )
{
  // Plausi test
  if( 2 > route.size()
    || false == m_bActive )
    return;

  BRN_DEBUG("inserting route %s -> %s.",
    addrSrc.unparse().c_str(), addrDst.unparse().c_str());
  
  EntryType entry;
  AddressPairType pairSrcDst(addrSrc,addrDst);
  
  entry.m_route = route;
  entry.m_iTTL  = m_iInitialTTL;
  
  // Insert the route under key pair(src,dst)
  m_mapRoutes.insert( pairSrcDst, entry );
  
  // Insert all links into the map, pointing to the route
  RouteType::const_iterator iter_a = route.begin();
  RouteType::const_iterator iter_b = iter_a + 1;
  for( ;iter_b != route.end(); ++iter_a, ++iter_b )
  {
    // Get the corresponding route vector, create if necessary
    AddrPairVectorType* pLinkVector = 
      m_mapLinkToRoute.findp( AddressPairType(*iter_a,*iter_b) );
    if( NULL == pLinkVector )
    {
      m_mapLinkToRoute.insert( AddressPairType(*iter_a,*iter_b),
        AddrPairVectorType() );
      pLinkVector = m_mapLinkToRoute.findp( AddressPairType(*iter_a,*iter_b) );
    }

    // Search the route keys in link vector    
    AddrPairVectorType::iterator iter_link = pLinkVector->begin();
    while( iter_link != pLinkVector->begin() 
      && *iter_link != pairSrcDst )
      ++iter_link;
    if( iter_link == pLinkVector->end() )
    {
      BRN_DEBUG("associate route %s -> %s with link %s -> %s.",
        addrSrc.unparse().c_str(), addrDst.unparse().c_str(), 
        iter_a->unparse().c_str(), iter_b->unparse().c_str() );

      pLinkVector->push_back( pairSrcDst );
    }
  }
}

////////////////////////////////////////////////////////////////////////
void
BrnRouteCache::flush_cache()
{
  m_mapLinkToRoute.clear();
  m_mapRoutes.clear();
}

////////////////////////////////////////////////////////////////////////
void 
BrnRouteCache::remove_route( 
  /*[in]*/  const AddressType& addrSrc,
  /*[in]*/  const AddressType& addrDst )
{
  AddressPairType pairSrcDst(addrSrc,addrDst);

  BRN_DEBUG("removing route %s -> %s.",
    addrSrc.unparse().c_str(), addrDst.unparse().c_str());

  // Remove the mappings link to route
  EntryType* pEntry = m_mapRoutes.findp( pairSrcDst );
  if( NULL == pEntry )
    return;

  // Loop through all links along the route
  const RouteType& route = pEntry->m_route;
  RouteType::const_iterator iter_a = route.begin();
  RouteType::const_iterator iter_b = iter_a + 1;
  for( ;iter_a != route.end(); iter_a++, iter_b++ )
  {
    // Get the corresponding route vector for the link
    AddrPairVectorType* pLinkVector = 
      m_mapLinkToRoute.findp( AddressPairType(*iter_a,*iter_b) );
    if( NULL == pLinkVector )
      continue; // Strange thing that no route vector is avail, but...
      
    // Loop through vector and delete the current route entry 
    AddrPairVectorType::iterator iter_link = pLinkVector->begin();
    while( *iter_link != pairSrcDst )
      ++iter_link;
    
    // We found the route entry in the vector
    if( *iter_link == pairSrcDst )
    {
      pLinkVector->erase(iter_link);
    }
    
    // Cleanup, remove LinkToRoute entry, if not used anymore
    if( true == pLinkVector->empty() )
    {
      m_mapLinkToRoute.remove( AddressPairType(*iter_a,*iter_b) );
    }
  }

  // Remove the route from route map
  m_mapRoutes.remove( pairSrcDst );
}

////////////////////////////////////////////////////////////////////////
void 
BrnRouteCache::on_link_changed(
  /*[in]*/ const AddressType& addrLinkNodeA,
  /*[in]*/ const AddressType& addrLinkNodeB )
{
  if( false == m_bActive )
    return;

  AddressPairType addrPairAB(addrLinkNodeA,addrLinkNodeB);
  
  BRN_DEBUG("link %s -> %s changed.",
    addrLinkNodeA.unparse().c_str(), addrLinkNodeB.unparse().c_str());

  // Find the route vector for the link
  AddrPairVectorType* pLinkVector = m_mapLinkToRoute.findp( addrPairAB );

  // We do not have information about the link, so go home...
    // Be aware that both iterators are not stable!!
  while( NULL != pLinkVector )
  {  
    // Loop through vector ...
    AddrPairVectorType::iterator iter_link = pLinkVector->begin();
    if( iter_link == pLinkVector->end() )
    {
      // Remove the empty link vector
      m_mapLinkToRoute.remove( addrPairAB );
      
      /// @TODO error message??
      return;
    }

    BRN_DEBUG("flushing affected route %s -> %s.",
      iter_link->first.unparse().c_str(), iter_link->second.unparse().c_str() );

    // ...and delete all associated routes
    remove_route( iter_link->first, iter_link->second );

    // The link vector could have been deleted in remove_route...
    pLinkVector = m_mapLinkToRoute.findp( addrPairAB );
  }
}

////////////////////////////////////////////////////////////////////////
void 
BrnRouteCache::on_routeaging_expired( 
  /*[in]*/  Timer* pTimer, 
  /*[in]*/  void*  pVoid )
{
  if( NULL == pTimer
    || NULL == pVoid )
  {
    /// @TODO error message
    return;
  }
  
  BrnRouteCache* pThis = static_cast<BrnRouteCache*>(pVoid);
  pThis->on_routeaging_expired( pTimer );
}

////////////////////////////////////////////////////////////////////////
void 
BrnRouteCache::on_routeaging_expired( 
  /*[in]*/  Timer* pTimer )
{
  if( false == m_bActive )
    return;

  // Go through all routes and dec the ttl
  RouteMapType::iterator iter = m_mapRoutes.begin();
  while( iter != m_mapRoutes.end() )
  {
    // Remember the current and go to next.
    RouteMapType::iterator iter_curr = iter; ++iter;
    
    // Decrement the ttl
    iter_curr.value().m_iTTL -= 1;
    
    // If the lifetime is expired, delete the route entry
    if( 0 >= iter_curr.value().m_iTTL )
    {
      remove_route( iter_curr.key().first, iter_curr.key().second );
    }
  }
  
  // Reschedule the timer
  pTimer->reschedule_after( m_tvLifetimeSlice );
}

////////////////////////////////////////////////////////////////////////
// Handler
////////////////////////////////////////////////////////////////////////
static String
read_debug_param(Element *e, void *thunk)
{
  UNREFERENCED_PARAMETER(thunk);
  
  BrnRouteCache *rq = (BrnRouteCache *)e;
  return String(rq->_debug) + "\n";
}

////////////////////////////////////////////////////////////////////////
static int 
write_debug_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(vparam);

  BrnRouteCache *rq = (BrnRouteCache *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  rq->_debug = debug;
  return 0;
}

////////////////////////////////////////////////////////////////////////
static String
read_active_param(Element *e, void *thunk)
{
  UNREFERENCED_PARAMETER(thunk);
  
  BrnRouteCache *rq = (BrnRouteCache *)e;
  return String(rq->m_bActive) + "\n";
}

////////////////////////////////////////////////////////////////////////
static int 
write_active_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(vparam);

  BrnRouteCache *rq = (BrnRouteCache *)e;
  String s = cp_uncomment(in_s);
  bool active;
  if (!cp_bool(s, &active)) 
    return errh->error("active parameter must be boolean");
  rq->m_bActive = active;
  rq->flush_cache();

  if( true == active 
    && rq->m_tvLifetimeSlice.tv_sec < 1000000 )
  {
    rq->m_tRouteAging.schedule_after( rq->m_tvLifetimeSlice );
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////
void
BrnRouteCache::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);

  add_read_handler("active", read_active_param, 0);
  add_write_handler("active", write_active_param, 0);
}

////////////////////////////////////////////////////////////////////////

#include <click/bighashmap.cc>
#include <click/hashmap.cc>
#include <click/vector.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
template class Pair<BrnRouteCache::AddressType,BrnRouteCache::AddressType>;
template class Vector<BrnRouteCache::AddressType>;
template class HashMap<BrnRouteCache::AddressPairType,BrnRouteCache::EntryType>;
template class Vector<BrnRouteCache::AddressPairType>;
template class HashMap<BrnRouteCache::AddressPairType,BrnRouteCache::AddrPairVectorType>;
#endif


////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnRouteCache)
ELEMENT_MT_SAFE(BrnRouteCache)

////////////////////////////////////////////////////////////////////////
