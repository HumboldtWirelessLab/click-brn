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
#include "brnetxmetric.hh"
#include "brnlinkstat.hh"

CLICK_DECLS 

BRNETXMetric::BRNETXMetric()
  : GenericMetric(), 
    _link_table(0),
    _debug(BrnLogger::DEFAULT)
{
}

BRNETXMetric::~BRNETXMetric()
{
}

void *
BRNETXMetric::cast(const char *n) 
{
  if (strcmp(n, "BRNETXMetric") == 0)
    return (BRNETXMetric *) this;
  else if (strcmp(n, "LinkMetric") == 0)
    return (GenericMetric *) this;
  else
    return 0;
}

int
BRNETXMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
    cpKeywords,
    "LT", cpElement, "LinkTable element", &_link_table, 
    cpEnd);
  if (res < 0)
    return res;
  if (_link_table == 0) {
    BRN_FATAL(" no LTelement specified");
  }
  if (_link_table && _link_table->cast("BrnLinkTable") == 0) {
    return errh->error("BrnLinkTable element is not a BrnLinkTable");
  }
  return 0;
}

void
BRNETXMetric::update_link(EtherAddress from, EtherAddress to, 
		       Vector<BrnRateSize>, 
		       Vector<int> fwd, Vector<int> rev, 
		       uint32_t seq)
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
    BRN_WARN(" couldn't update link %s > %d > %s\n", from.s().c_str(), metric, to.s().c_str());
  }
  if (metric && 
      _link_table && 
      !_link_table->update_link(to, from, seq, 0, metric)){
    BRN_WARN(" couldn't update link %s < %d < %s\n", from.s().c_str(), metric, to.s().c_str());
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRNETXMetric *etx = (BRNETXMetric *)e;
  return String(etx->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRNETXMetric *etx = (BRNETXMetric *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  etx->_debug = debug;
  return 0;
}

void
BRNETXMetric::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(BRNETXMetric)

#include <click/vector.cc>
template class Vector<BrnRateSize>;

CLICK_ENDDECLS
