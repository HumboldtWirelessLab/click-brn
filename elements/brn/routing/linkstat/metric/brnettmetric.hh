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

#ifndef BRNBRNETTMetric_HH
#define BRNBRNETTMetric_HH
#include <click/element.hh>
#include "genericmetric.hh"
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>
#include <elements/wifi/bitrate.hh>
#include "elements/brn/routing/linkstat/brnlinkstat.hh"
CLICK_DECLS

/*
 * =c
 * BRNETTMetric
 * =s Wifi
 * Estimated Transmission Time (ETT) metric
 * 
 * =io
 * None
 *
 */
inline unsigned ett2_metric(int ack_prob, int data_prob, int data_rate) 
{
  if (!ack_prob || ! data_prob) {
    return 0;
  }
  int retries = 100 * 100 * 100 / (ack_prob * data_prob) - 100;
  unsigned low_usecs = calc_usecs_wifi_packet(1500, data_rate, retries/100);
  unsigned high_usecs = calc_usecs_wifi_packet(1500, data_rate, (retries/100) + 1);

  click_chatter("low_usecs = %d, high_usecs = %d\n", low_usecs, high_usecs);

  unsigned diff = retries % 100;
  unsigned average = (diff * high_usecs + (100 - diff) * low_usecs) / 100;
  return average;
}

class BRNLinkStat;
class BrnRateSize;

class BRNETTMetric : public GenericMetric {

public:

  BRNETTMetric();
  ~BRNETTMetric();
  const char *class_name() const { return "BRNETTMetric"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  void *cast(const char *);

  static String read_stats(Element *xf, void *);

  void update_link(EtherAddress from, EtherAddress to, 
    Vector<BrnRateSize> rs, Vector<int> fwd, Vector<int> rev, uint32_t seq);

  int get_tx_rate(EtherAddress);

private:
  class BrnLinkTable *_link_table;

public:
  int _debug;
};

CLICK_ENDDECLS
#endif
