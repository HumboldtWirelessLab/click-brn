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

#ifndef BRN2ETXMETRIC_HH
#define BRN2ETXMETRIC_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "brn2_genericmetric.hh"

#include <elements/wifi/bitrate.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

/*
 * =c
 * BRN2ETXMetric(LinkStat, LinkStat)
 * =s Wifi
 * The Estimated Transmission Count metric (ETX).
 * 
 * The ETX of a link is calculated using the forward and reverse
 * delivery ratios of the link. The forward delivery ratio, df , is the
 * measured probability that a data packet successfully arrives at the
 * recipient; the reverse delivery ratio, dr , is the probability that the
 * ACK packet is successfully received. These delivery ratios can
 * be measured as described below. The expected probability that a
 * transmission is successfully received and acknowledged is df  dr .
 * A sender will retransmit a packet that is not successfully acknowledged.
 * Because each attempt to transmit a packet can be considered
 * a Bernoulli trial, the expected number of transmissions is:
 */
inline unsigned brn2txcount2_metric(int ack_prob, int data_prob, int data_rate) 
{
  if (!ack_prob || ! data_prob) {
    return 0;
  }
  int retries = 100 * 100 * 100 / (ack_prob * data_prob) - 100;
  unsigned low_usecs = calc_usecs_wifi_packet(1500, data_rate, retries/100);
  unsigned high_usecs = calc_usecs_wifi_packet(1500, data_rate, (retries/100) + 1);

  unsigned diff = retries % 100;
  unsigned average = (diff * high_usecs + (100 - diff) * low_usecs) / 100;
  return average;
}

class BRN2ETXMetric : public BRN2GenericMetric {

public:

  BRN2ETXMetric();
  ~BRN2ETXMetric();
  const char *class_name() const { return "BRN2ETXMetric"; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  void *cast(const char *);

  static String read_stats(Element *xf, void *);

  void update_link(EtherAddress from, EtherAddress to, Vector<BrnRateSize> rs,
                   Vector<uint8_t> fwd, Vector<uint8_t> rev, uint32_t seq,
                   uint8_t update_mode);

 private:
  Brn2LinkTable *_link_table;

};

CLICK_ENDDECLS
#endif
