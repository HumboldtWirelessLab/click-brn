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

#include "elements/brn/brn2.h"
#include "elements/brn/brnelement.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/routing/identity/brn2_device.hh"

#include "channelstats.hh"
#include "cooperativechannelstats_protocol.hh"
#include "nodechannelstats.hh"


CLICK_DECLS

/*
 =c
 CooperativeChannelStats()

 =d

 =a

 */

class CooperativeChannelStats : public BRNElement
{
  public:

    CooperativeChannelStats();
    ~CooperativeChannelStats();

    const char *class_name() const { return "CooperativeChannelStats"; }
    const char *processing() const { return PUSH; }
    const char *port_count() const { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void run_timer(Timer *);

    void add_handlers();

    void push(int, Packet *p);

    void send_message();  /* called by run_timer (creates msg and send it. */

    String stats_handler(int mode); /* print info */

    NodeChannelStatsMap *get_stats_map() { return &_ncst; }
    NodeChannelStats *get_stats(EtherAddress *);

    int get_neighbours_with_max_age(int age, Vector<EtherAddress> *ea_vec);

  private:

    NodeChannelStatsMap _ncst;  /* stats of neighbouring node */

    ChannelStats *_cst;         /* own stats. use to send info */

    Timer _msg_timer;
    uint32_t _interval;
    bool _add_neighbours; //add neighbours to packet?
};

CLICK_ENDDECLS
#endif
