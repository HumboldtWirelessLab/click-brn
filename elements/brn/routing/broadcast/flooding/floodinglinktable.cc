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
    _pdrlinktable(NULL)
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
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  if ( !_linkstat && !_etxlinktable && !_pdrlinktable )
    return errh->error("FloodingLinktable requires at least LINKSTAT, ETXLINKTABLE or PDRLINKTABLE!");

  return 0;
}

int
FloodingLinktable::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

int
FloodingLinktable::get_pdr(EtherAddress &src, EtherAddress &dst)
{
  int metric = BRN_LT_INVALID_LINK_METRIC;
  /* Since _pdrlinktable includes all links we prefere it */
  if ( _pdrlinktable ) {
    metric = _pdrlinktable->get_link_metric(src, dst);
    return metric >> 6;
  } else {
    /* if we don't have the pdr table we prefere linkstats (more precise)*/
    if (_linkstat) {
      if (*(_linkstat->_dev->getEtherAddress()) == dst) {
        metric = _linkstat->get_rev_rate(&src);
      } else if (*(_linkstat->_dev->getEtherAddress()) == src) {
        metric = _linkstat->get_fwd_rate(&dst);
      }
    }
    if ( metric != BRN_LT_INVALID_LINK_METRIC ) return metric;
  }

  /* but if it is a nonlocal link we try the etxlinktable */
  if ( _etxlinktable ) {
    return etx_metric2pdr(_etxlinktable->get_link_metric(src, dst));
  }

  if ( metric == BRN_LT_INVALID_LINK_METRIC) return 0;

  return metric;
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
