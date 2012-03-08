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
 * @file brnroutingtable.hh
 * @brief Declaration of the BrnRoutingTable class.
 */

#ifndef BRNROUTINGTABLE_HH_
#define BRNROUTINGTABLE_HH_

#include <click/glue.hh>
#include <click/timer.hh>

#include <click/straccum.hh>
#include <click/hashmap.hh>
#include <click/pair.hh>
#include <click/vector.hh>

#include <click/error.hh>
#include <click/element.hh>

#include <click/etheraddress.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////

/**
 * @brief Table of routes.
 *
 * @note Parameters
 *  - DEBUG: Debug indicator (Write debug messages),
 *  - ACTIVE: Whether to use the cache or not,
 *  - DROP: Route flush probability is 1/DROP, 0 disables random route
 *          flushes
 *  - TTL: Initial route ttl in slices
 *  - SLICE: Lifetime slice in us, 0 disables aging
 *
 */
class BrnRoutingTable : public BRNElement
{
//----------------------------------------------------------------------
// Types
//----------------------------------------------------------------------
public:
  typedef EtherAddress                                      AddressType;
  typedef Pair<AddressType,AddressType>                 AddressPairType;
  typedef Vector<AddressType>                                 RouteType;
  typedef struct tagEntryType {
    RouteType m_route;
    int32_t   m_iTTL;
    uint32_t  m_metric;
  }                                                           EntryType;
  typedef HashMap<AddressPairType,EntryType*>               RouteMapType;

  typedef Vector<AddressPairType>                    AddrPairVectorType;
  typedef HashMap<AddressPairType,AddrPairVectorType>  LinkRouteMapType;

//----------------------------------------------------------------------
// Construction
//----------------------------------------------------------------------
 public:
  BrnRoutingTable();
  virtual ~BrnRoutingTable();

  const char* class_name() const { return "BrnRoutingTable"; }

  void add_handlers();
  int initialize(ErrorHandler *);
  int configure(Vector<String> &conf, ErrorHandler *errh);

//----------------------------------------------------------------------
// Operations
//----------------------------------------------------------------------
public:
  /**
   * @brief Query the cache for a route between addrSrc and addrDst.
   * @param addrSrc @a [in] The source of the route.
   * @param addrDst @a [in] The destination of the route.
   * @param route @a [in] Holds the route, if found.
   * @return true if a cached route was found, false otherwise.
   */
  bool get_cached_route(
    /*[in]*/  const AddressType&  addrSrc,
    /*[in]*/  const AddressType&  addrDst,
    /*[out]*/ RouteType&          route,
              uint32_t *metric);

  /**
   * @brief Inserts a route into the route cache.
   * @param addrSrc @a [in] The source of the route.
   * @param addrDst @a [in] The destination of the route.
   * @param route @a [in] The route to cache.
   */
  void insert_route(
    /*[in]*/ const AddressType& addrSrc,
    /*[in]*/ const AddressType& addrDst,
    /*[in]*/ const RouteType&   route,
             uint32_t metric);

  /**
   * @brief Inform the cache about a link change.
   * @note The cache flushes all routes containing the link to update.
   * Links are considered bidirectional.
   * @param addrLinkNodeA @a [in] The source of the route.
   * @param addrLinkNodeB @a [in] The destination of the route.
   */
  void on_link_changed(
    /*[in]*/ const AddressType& addrLinkNodeA,
    /*[in]*/ const AddressType& addrLinkNodeB );

  /**
   * @brief Clears the cache.
   */
  void flush_cache();

  String print_stats();

//----------------------------------------------------------------------
// Helpers
//----------------------------------------------------------------------
protected:
  /**
   * @brief Removes the route specified by src and dst from cache.
   * @param addrSrc @a [in] The start point of the route.
   * @param addrDst @a [in] The end point of the route.
   */
  void remove_route(
    /*[in]*/  const AddressType& addrSrc,
    /*[in]*/  const AddressType& addrDst );

  /**
   * @brief Timer callback for m_tRouteAging timer.
   * @param pTimer @a [in] The expired timer.
   * @param pVoid @a [in] Reference to the route cache object.
   */
  static void on_routeaging_expired(
    /*[in]*/  Timer* pTimer,
    /*[in]*/  void*  pVoid );

  void on_routeaging_expired(
    /*[in]*/  Timer* pTimer );

//----------------------------------------------------------------------
// Data
//----------------------------------------------------------------------
public:
  bool    m_bActive; ///< Whether the cache should be used.
  Timer   m_tRouteAging; ///< Timer for aging the routes.
  int m_tvLifetimeSlice; ///< Time slice for the entry life timer

private:
  int     m_iInitialTTL; ///< Initial value of the entriy TTL 
  int     m_iDropProb; ///< drop probability = 1/m_iDropProb

  RouteMapType     m_mapRoutes; ///< maps src,dst pairs to routes
  LinkRouteMapType m_mapLinkToRoute; ///< maps links to routes
};

////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
#endif /*BRNROUTINGTABLE_HH_*/
