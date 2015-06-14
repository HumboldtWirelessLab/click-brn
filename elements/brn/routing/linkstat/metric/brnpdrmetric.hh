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

#ifndef BRNPDRMETRIC_HH
#define BRNPDRMETRIC_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "brn2_genericmetric.hh"

#include <elements/wifi/bitrate.hh>
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

/*
 * =c
 * BRNPDRMetric(LinkStat, LinkStat)
 * =s Wifi
 * The Packet Del. Ratio metric (PDR).
 *
 * TODO: Metric is not useful for add!! use mul instead. 
 */
class BRNPDRMetric : public BRN2GenericMetric {

public:

  BRNPDRMetric();
  ~BRNPDRMetric();
  const char *class_name() const { return "BRNPDRMetric"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  void *cast(const char *);

  static String read_stats(Element *xf, void *);

  void update_link(const EtherAddress &from, EtherAddress &to, Vector<BrnRateSize> &rs,
                   Vector<uint8_t> &fwd, Vector<uint8_t> &rev, uint32_t seq,
                   uint8_t update_mode);

  static inline void get_pdr(uint32_t metric, uint8_t *fwd, uint8_t *rev) {
    *fwd = metric >> 6;
    *rev = (metric & 127) << 1;
  }

 private:
  Brn2LinkTable *_link_table;

};

CLICK_ENDDECLS
#endif
