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

#ifndef CLICK_COOPERATIVECHANNELSTATS_HH
#define CLICK_COOPERATIVECHANNELSTATS_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"
#include "cooperativechannelstats.hh"
#include "channelstats.hh"

CLICK_DECLS

/*
=c
CooperativeChannelStats()

=d

=a

*/

#define ENDIANESS_TEST 0x1234

struct cooperative_channel_stats_header {
  uint16_t endianess;
  uint16_t reserved; //alignment to 32 bit (faster memcpy of airtime_stats)
};

class CooperativeChannelStats : public BRNElement {

  private:
    class NodeChannelStats {
     public:
      EtherAddress node;
      struct airtime_stats stats;
      uint16_t _endianess;

      NodeChannelStats() {
        node = EtherAddress();
      }

      NodeChannelStats(EtherAddress ea) {
        node = ea;
      }

      void set_stats(struct airtime_stats *new_stats, uint16_t endianess) {
        memcpy(&stats, new_stats, sizeof(struct airtime_stats));
        _endianess = endianess;
      }
    };

    typedef HashMap<EtherAddress, NodeChannelStats*> NodeChannelStatsTable;
    typedef NodeChannelStatsTable::const_iterator NodeChannelStatsTableIter;

  public:

    CooperativeChannelStats();
    ~CooperativeChannelStats();

    const char *class_name() const	{ return "CooperativeChannelStats"; }
    const char *processing() const  { return PUSH; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);
    int initialize(ErrorHandler *);
    void run_timer(Timer *);

    void add_handlers();

    void push(int, Packet *p);

    String stats_handler(int mode);

    void send_message();

  private:
    ChannelStats *_cst;

    Timer _msg_timer;

    uint32_t _interval;

    NodeChannelStatsTable ncst;

};

CLICK_ENDDECLS
#endif
