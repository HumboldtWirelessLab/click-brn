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
 * brnettmetric.{cc,hh} -- expected transmission count (`ETT') metric
 * A. Zubow
 *
 * Adapted from wifi/sr/ettmetric.{cc,hh} to brn (many thanks to John Bicket).
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include "brnettmetric.hh"

CLICK_DECLS 

BRNETTMetric::BRNETTMetric()
  : BRN2GenericMetric(),
    _link_table(0)
{
  BRNElement::init();
}

BRNETTMetric::~BRNETTMetric()
{
}

void *
BRNETTMetric::cast(const char *n) 
{
  if (strcmp(n, "BRNETTMetric") == 0)
    return (BRNETTMetric *) this;
  else if (strcmp(n, "BRN2GenericMetric") == 0)
    return (BRN2GenericMetric *) this;
  else
    return 0;
}

int
BRNETTMetric::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_kparse(conf, this, errh,
    "LT", cpkP+cpkM, cpElement, &_link_table,
    cpEnd);

  if (!_link_table || _link_table->cast("Brn2LinkTable") == 0) 
    return errh->error("Brn2LinkTable element is not provided or not a BrnLinkTable");

  return res;
}


int 
BRNETTMetric::get_tx_rate(EtherAddress &) 
{
  return 2;
}

void
BRNETTMetric::update_link(const EtherAddress &from, EtherAddress &to, Vector<BrnRateSize> &rs,
                          Vector<uint8_t> &fwd, Vector<uint8_t> &rev, uint32_t seq,
                          uint8_t update_mode)
{

  if (!from || !to) {
    click_chatter("%{element}::update_link called with %s %s\n",
      this, from.unparse().c_str(), to.unparse().c_str());
    return;
  }

  int one_ack_fwd = 0;
  int one_ack_rev = 0;
  int six_ack_fwd = 0;
  int six_ack_rev = 0;

  /* 
   * if we don't have a few probes going out, just pick
   * the smallest size for fwd rate
   */
  int one_ack_size = 0;
  int six_ack_size = 0;

  for (int x = 0; x < rs.size(); x++) {
    if (rs[x]._rate == 2 && (!one_ack_size || one_ack_size > rs[x]._size)) {
      one_ack_size = rs[x]._size;
      one_ack_fwd = fwd[x];
      one_ack_rev = rev[x];
    } else if (rs[x]._rate == 12 && (!six_ack_size || six_ack_size > rs[x]._size)) {
      six_ack_size = rs[x]._size;
      six_ack_fwd = fwd[x];
      six_ack_rev = rev[x];
    }
  }

  if (!one_ack_fwd && !six_ack_fwd &&
      !one_ack_rev && !six_ack_rev) {
    return;
  }

  int rev_metric = 0;
  int fwd_metric = 0;
  //int best_rev_rate = 0;
  //int best_fwd_rate = 0;

  for (int x = 0; x < rs.size(); x++) {
    if (rs[x]._size >= 100) {
      int ack_fwd = 0;
      int ack_rev = 0;
      if ((rs[x]._rate == 2) || (rs[x]._rate == 4) || (rs[x]._rate == 11) || (rs[x]._rate == 22)) {
        ack_fwd = one_ack_fwd;
        ack_rev = one_ack_rev;
      } else {
        ack_fwd = six_ack_fwd;
        ack_rev = six_ack_rev;
      }
      int metric = ett2_metric(ack_rev, fwd[x], rs[x]._rate);
      //click_chatter("METRIC = %d\n", metric);
      if (!fwd_metric || (metric && metric < fwd_metric)) {
        //best_fwd_rate = rs[x]._rate;
        fwd_metric = metric;
      }

      metric = ett2_metric(ack_fwd, rev[x], rs[x]._rate);

      if (!rev_metric || (metric && metric < rev_metric)) {
        rev_metric = metric;
        //best_rev_rate= rs[x]._rate;
      }
    }
  }

  /* update linktable */
  if (fwd_metric) {
    if (!_link_table->update_link(from, to, seq, 0, fwd_metric, update_mode)) {
      click_chatter("%{element} couldn't update link %s > %d > %s\n",
                    this, from.unparse().c_str(), fwd_metric, to.unparse().c_str());
    } else {
      //click_chatter("XXX: %{element} update link %s > %d > %s\n",
      //              this, from.unparse().c_str(), fwd_metric, to.unparse().c_str());
    }
  }
  if (rev_metric) {
    if (rev_metric > 0) {
      //click_chatter("%d", rev_metric);
      //exit(-1);
    }
    if (!_link_table->update_link(to, from, seq, 0, rev_metric, update_mode)) {
      click_chatter("%{element} couldn't update link %s < %d < %s\n",
                    this, from.unparse().c_str(), rev_metric, to.unparse().c_str());
    } else {
      //click_chatter("XXX: %{element} update link %s < %d < %s\n",
      //              this, from.unparse().c_str(), rev_metric, to.unparse().c_str());
    }
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
BRNETTMetric::add_handlers()
{
  BRNElement::add_handlers();
}

EXPORT_ELEMENT(BRNETTMetric)

#include <click/vector.cc>
template class Vector<BrnRateSize>;

CLICK_ENDDECLS
