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
#define INCLUDES_NEIGHBOURS 1

struct cooperative_channel_stats_header {

    uint16_t endianess;     // endianess-test
    uint8_t flags;          // flags
    uint8_t no_neighbours;  // number of neighbours
};

struct cooperative_message_body {
    
    struct cooperative_channel_stats_header ccsh;
    struct neighbour_airtime_stats *nats_arr;
};

class CooperativeChannelStats : public BRNElement {
    
    typedef HashMap<EtherAddress, struct neighbour_airtime_stats*> NeighbourStatsTable;
    typedef NeighbourStatsTable::const_iterator NeighbourStatsTableIter;

private:

    class NodeChannelStats {

    private:
        struct airtime_stats stats;

    public:
        EtherAddress node;
        uint16_t _endianess;
        bool _is_fix_endianess;
        NeighbourStatsTable _n_stats;

        NodeChannelStats() {

            node = EtherAddress();
            _is_fix_endianess = false;
        }

        NodeChannelStats(EtherAddress ea) {

            node = ea;
            _is_fix_endianess = false;
        }

        void set_stats(struct airtime_stats *new_stats, uint16_t endianess) {

            memcpy(&stats, new_stats, sizeof(struct airtime_stats));
            _endianess = endianess;
            _is_fix_endianess = false;
        }

        struct airtime_stats *get_airtime_stats() {

            if ( ! _is_fix_endianess ) {
                //fix it
                _is_fix_endianess = true;
            }
            return &stats;
        }

        void add_neighbour_stats(EtherAddress *ea, struct neighbour_airtime_stats *stats) {

            _n_stats.insert(*ea, stats);
        }
    };

    typedef HashMap<EtherAddress, NodeChannelStats*> NodeChannelStatsTable;
    typedef NodeChannelStatsTable::const_iterator NodeChannelStatsTableIter;

public:

    class CooperativeStatsCircularBuffer {

    private:
        uint32_t size;
        uint32_t start_elem;
        uint32_t counter;
        HashMap<EtherAddress, neighbour_airtime_stats>* time_buffer;

    public:

    };

    // todo: pro nachbar zeitarray mit native airtime stats um vergangene kanalauslastung auswerten zu können
    CooperativeChannelStats();
    ~CooperativeChannelStats();

    const char *class_name() const  { return "CooperativeChannelStats"; }
    const char *processing() const  { return PUSH; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);
    int initialize(ErrorHandler *);
    void run_timer(Timer *);

    void add_handlers();

    void push(int, Packet *p);

    String stats_handler(int mode);

    void send_message();
    
    HashMap<EtherAddress, struct neighbour_airtime_stats*> get_stats(EtherAddress *);
    
private:
    ChannelStats *_cst;

    Timer _msg_timer;

    uint32_t _interval;

    NodeChannelStatsTable ncst;

    bool _add_neighbours;
};

CLICK_ENDDECLS
#endif
