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
#include <click/config.h>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>
#include <click/error.hh>
#include <click/userutils.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brn2.h"
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/brnelement.hh"
#include "channelstats.hh"
#include "packetlossestimationhelper/cooperativestatscircularbuffer.hh"
#include "packetlossestimationhelper/nodechannelstats.hh"

CLICK_DECLS

/*
=c
CooperativeChannelStats()

=d

=a

*/

#define INCLUDES_NEIGHBOURS 1

struct cooperative_channel_stats_header
{
    uint16_t    endianess;      // endianess-test
    uint8_t     flags;          // flags
    uint8_t     no_neighbours;  // number of neighbours
};

struct cooperative_message_body
{    
    struct cooperative_channel_stats_header ccsh;
    struct neighbour_airtime_stats          *nats_arr;
};

class CooperativeChannelStats : public BRNElement
{
public:
    
	HashMap<EtherAddress, CooperativeStatsCircularBuffer*> neighbours_airtime_stats_history;
    
    CooperativeChannelStats ();
    virtual ~CooperativeChannelStats ();

    const char *class_name () const { return "CooperativeChannelStats"; }
    const char *processing () const { return PUSH; }
    const char *port_count () const { return "1/1"; }

    int configure (Vector<String> &, ErrorHandler *);
    int initialize (ErrorHandler *);
    void run_timer (Timer *);

    void add_handlers ();

    void push (int, Packet *p);

    String stats_handler (int mode);

    void send_message ();
    
    HashMap<EtherAddress, struct neighbour_airtime_stats*> get_stats (EtherAddress *);
    
private:
    typedef HashMap<EtherAddress, struct neighbour_airtime_stats*>  NeighbourStatsTable;
    typedef NeighbourStatsTable::const_iterator                     NeighbourStatsTableIter;
    typedef HashMap<EtherAddress, NodeChannelStats*>                NodeChannelStatsTable;
    typedef NodeChannelStatsTable::const_iterator                   NodeChannelStatsTableIter;

    static CooperativeStatsCircularBuffer 	_coop_stats_buffer;
    ChannelStats            				*_cst;
    Timer                   				_msg_timer;
    uint32_t                				_interval;
    NodeChannelStatsTable   				_ncst;
    bool                    				_add_neighbours;

    WritablePacket *create_new_packet (struct cooperative_channel_stats_header, struct neighbour_airtime_stats []);
};

CLICK_ENDDECLS
#endif
