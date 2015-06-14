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
 * floodinglinktable.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "floodinglinktable.hh"

CLICK_DECLS

//TODO: more generic (not only ETX, use Linkstats instead or ask metric-class
uint32_t
FloodingLinktable::etx_metric2pdr(uint32_t metric)
{
  assert( metric <= BRN_LT_INVALID_LINK_METRIC );
  if ( metric == 0 ) return 100;
  if ( metric == BRN_LT_INVALID_LINK_METRIC ) return 0;

  return (1000 / isqrt32(metric));
}


FloodingLinktable::FloodingLinktable():
    _linkstat(NULL),
    _etxlinktable(NULL),
    _pdrlinktable(NULL),
    _locallinktable(NULL),
    _pref_mode(0),
    _used_mode(0)
{
  BRNElement::init();
}

FloodingLinktable::~FloodingLinktable()
{
}

int
FloodingLinktable::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKSTAT", cpkP, cpElement, &_linkstat,
      "ETXLINKTABLE", cpkP, cpElement, &_etxlinktable,
      "PDRLINKTABLE", cpkP, cpElement, &_pdrlinktable,
      "LOCALTABLE", cpkP, cpElement, &_locallinktable,
      "MODE", cpkP, cpInteger, &_pref_mode,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  if ( _pref_mode != 0 ) {      //User set pref-mode: remove what we can not provide
    _used_mode = _pref_mode;
    if (!_linkstat) _used_mode &= ~FLOODING_LT_MODE_LINKSTATS;
    if (!_etxlinktable) _used_mode &= ~FLOODING_LT_MODE_ETX;
    if (!_pdrlinktable) _used_mode &= ~FLOODING_LT_MODE_PDR;
    if (!_locallinktable) _used_mode &= ~FLOODING_LT_MODE_LOCAL;
  } else {                      //not prefs so we take whats there
    if (_linkstat) _used_mode |= FLOODING_LT_MODE_LINKSTATS;
    if (_etxlinktable) _used_mode |= FLOODING_LT_MODE_ETX;
    if (_pdrlinktable) _used_mode |= FLOODING_LT_MODE_PDR;
    if (_locallinktable) _used_mode |= FLOODING_LT_MODE_LOCAL;
  }

  /*BRN_ERROR("MODE:\n ETX: %d\n PDR: %d\n LINKSTATS: %d\n LOCAL: %d\n", _used_mode & FLOODING_LT_MODE_ETX,
                                                                       _used_mode & FLOODING_LT_MODE_PDR,
                                                                       _used_mode & FLOODING_LT_MODE_LINKSTATS,
                                                                       _used_mode & FLOODING_LT_MODE_LOCAL);*/
  return 0;
}

int
FloodingLinktable::initialize(ErrorHandler */*errh*/)
{
  if (_linkstat) {
    _linkstat_addr = *(_linkstat->_dev->getEtherAddress());
  }
  return 0;
}

void
FloodingLinktable::get_neighbors(EtherAddress ethernet, Vector<EtherAddress> &neighbors)
{
  if (_etxlinktable) _etxlinktable->get_neighbors(ethernet, neighbors);
}

uint32_t
FloodingLinktable::get_link_metric(const EtherAddress from, const EtherAddress to)
{
  if (_etxlinktable) return _etxlinktable->get_link_metric(from, to);
  return BRN_LT_INVALID_LINK_METRIC;
}

uint32_t
FloodingLinktable::get_link_pdr(const EtherAddress &src, const EtherAddress &dst)
{
  //print_all_metrics(src, dst);

  int metric = BRN_LT_INVALID_LINK_METRIC;

  if (_used_mode & FLOODING_LT_MODE_LOCAL) {
    metric = _locallinktable->get_link_metric(src, dst);
    if ( metric != BRN_LT_INVALID_LINK_METRIC ) return metric >> 6;
    //if metric is invalid, the reason is maybe less information (no flooding yet,...)
  };

  /* Since _pdrlinktable includes all local links we prefere it */
  if (_used_mode & FLOODING_LT_MODE_PDR) {
    metric = _pdrlinktable->get_link_metric(src, dst);
    if ( metric != BRN_LT_INVALID_LINK_METRIC ) return metric >> 6;
  } else {
    /* if we don't have the pdr table we prefere linkstats (more precise)*/
    if (_used_mode & FLOODING_LT_MODE_LINKSTATS) {
      if (_linkstat_addr == dst) {
        return _linkstat->get_rev_rate(&src);
      } else if (_linkstat_addr == src) {
        return _linkstat->get_fwd_rate(&dst);
      }
    }
  }

  if (_used_mode & FLOODING_LT_MODE_ETX)
    return etx_metric2pdr(_etxlinktable->get_link_metric(src, dst));

  if ( metric == BRN_LT_INVALID_LINK_METRIC) return 0;

  return metric;
}

void
FloodingLinktable::print_all_metrics(const EtherAddress src, const EtherAddress dst)
{
  if (_locallinktable) BRN_DEBUG("LOCAL: %d", _locallinktable->get_link_metric(src, dst));
  if (_pdrlinktable) BRN_DEBUG("PDR: %d", _pdrlinktable->get_link_metric(src, dst));
  if (_etxlinktable) BRN_DEBUG("ETX: %d", etx_metric2pdr(_etxlinktable->get_link_metric(src, dst)));
  if (_linkstat) {
    if (_linkstat_addr == dst) {
      BRN_DEBUG("LINKSTATS: %d",_linkstat->get_rev_rate(&src));
    } else if (_linkstat_addr == src) {
      BRN_DEBUG("LINKSTATS: %d", _linkstat->get_fwd_rate(&dst));
    }
  }
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
FloodingLinktable::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingLinktable)
