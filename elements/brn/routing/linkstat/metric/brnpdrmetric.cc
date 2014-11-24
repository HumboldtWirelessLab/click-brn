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
 * brnpdrmetric.{cc,hh} -- pdr in both dirs
 *
 * R. Sombrutzki
 *
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include "brn2_genericmetric.hh"
#include "brnpdrmetric.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

BRNPDRMetric::BRNPDRMetric()
  : BRN2GenericMetric(),
    _link_table(0)
{
  BRNElement::init();
}

BRNPDRMetric::~BRNPDRMetric()
{
}

void *
BRNPDRMetric::cast(const char *n)
{
  if (strcmp(n, "BRNPDRMetric") == 0)
    return (BRNPDRMetric *) this;
  else if (strcmp(n, "BRN2GenericMetric") == 0)
    return (BRN2GenericMetric *) this;
  else
    return 0;
}

int
BRNPDRMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_kparse(conf, this, errh,
    "LT", cpkP+cpkM, cpElement, /*"LinkTable element",*/ &_link_table,
    cpEnd);

  if (res < 0) return res;

  if (_link_table && _link_table->cast("Brn2LinkTable") == 0) {
    return errh->error("Brn2LinkTable element is not a Brn2LinkTable");
  }
  return 0;
}

void
BRNPDRMetric::update_link(const EtherAddress &from, EtherAddress &to, Vector<BrnRateSize> &,
                           Vector<uint8_t> &fwd, Vector<uint8_t> &rev, uint32_t seq,
                           uint8_t update_mode)
{
  int metric = BRN_LT_INVALID_LINK_METRIC; //BRN_LT_INVALID_LINK_METRIC = 9999 => rev-rate=10% fwd-rate=10%

  if (fwd.size() && rev.size() && fwd[0] && rev[0]) {
    metric = ( (fwd[0] << 6) + (rev[0] >> 1)); // (fwd * 128 + rev) / 2 = fwd * 64 + rev/2
  }

  /*
  if (fwd.size() && rev.size() && (fwd[0] > 3) && (rev[0] > 3)) {
    metric = ((fwd[0]-2) * 100 + (rev[0] - 2));
  }
  */

  /* update linktable */
  if ( metric ) {
    if ( !_link_table->update_link(from, to, seq, 0, metric, update_mode) ) {
      BRN_WARN(" couldn't update link %s > %d > %s\n", from.unparse().c_str(), metric, to.unparse().c_str());
    }
    if (!_link_table->update_link(to, from, seq, 0, metric, update_mode)){
      BRN_WARN(" couldn't update link %s < %d < %s\n", from.unparse().c_str(), metric, to.unparse().c_str());
    }
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRNPDRMetric::add_handlers()
{
  BRNElement::add_handlers();
}

EXPORT_ELEMENT(BRNPDRMetric)
CLICK_ENDDECLS
