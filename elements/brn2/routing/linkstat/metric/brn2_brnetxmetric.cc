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
 * brnetxmetric.{cc,hh} -- estimated transmission count (`TXCount') metric
 *
 * A. Zubow
 *
 * Adapted from wifi/sr/txcountmetric.{cc,hh} to brn (many thanks to John Bicket).
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include "brn2_genericmetric.hh"
#include "brn2_brnetxmetric.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

CLICK_DECLS 

BRN2ETXMetric::BRN2ETXMetric()
  : BRN2GenericMetric(), 
    _link_table(0)
{
  BRNElement::init();
}

BRN2ETXMetric::~BRN2ETXMetric()
{
}

void *
BRN2ETXMetric::cast(const char *n)
{
  if (strcmp(n, "BRN2ETXMetric") == 0)
    return (BRN2ETXMetric *) this;
  else if (strcmp(n, "LinkMetric") == 0)
    return (BRN2GenericMetric *) this;
  else
    return 0;
}

int
BRN2ETXMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_kparse(conf, this, errh,
    "LT", cpkP+cpkM, cpElement, /*"LinkTable element",*/ &_link_table,
    cpEnd);
  if (res < 0)
    return res;
  if (_link_table == 0) {
    BRN_FATAL(" no LTelement specified");
  }
  if (_link_table && _link_table->cast("Brn2LinkTable") == 0) {
    return errh->error("Brn2LinkTable element is not a Brn2LinkTable");
  }
  return 0;
}

void
BRN2ETXMetric::update_link(EtherAddress from, EtherAddress to, Vector<BrnRateSize>,
                           Vector<uint8_t> fwd, Vector<uint8_t> rev, uint32_t seq)
{
  int metric = 9999;
  if (fwd.size() && rev.size() &&
      fwd[0] && rev[0]) {
    metric = 100 * 100 * 100 / (fwd[0] * rev[0]);
  }
  /* update linktable */
  if (metric && 
      _link_table && 
      !_link_table->update_link(from, to, seq, 0, metric)) {
    BRN_WARN(" couldn't update link %s > %d > %s\n", from.unparse().c_str(), metric, to.unparse().c_str());
  }
  if (metric && 
      _link_table && 
      !_link_table->update_link(to, from, seq, 0, metric)){
    BRN_WARN(" couldn't update link %s < %d < %s\n", from.unparse().c_str(), metric, to.unparse().c_str());
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRN2ETXMetric::add_handlers()
{
  BRNElement::add_handlers();
}

EXPORT_ELEMENT(BRN2ETXMetric)

#include <click/vector.cc>
template class Vector<BrnRateSize>;

CLICK_ENDDECLS
