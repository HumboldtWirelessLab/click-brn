/*
 * File:   cooperativestatscircularbuffer.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 24. Juli 2012, 16:09
 */

#ifndef COOPERATIVESTATSCIRCULARBUFFER_HH
#define	COOPERATIVESTATSCIRCULARBUFFER_HH

#include <click/config.h>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
#if CLICK_NS
#include <click/router.hh>
#endif
#include <click/hashmap.hh>
#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/channelstats.hh"
#include "elements/brn/wifi/packetlossestimationhelper/nodechannelstats.hh"

CLICK_DECLS

class CooperativeStatsCircularBuffer
{
public:
    CooperativeStatsCircularBuffer(uint32_t start_size);
    virtual ~CooperativeStatsCircularBuffer();
    void insert_values(NodeChannelStats &);

private:
    CooperativeStatsCircularBuffer();

    uint16_t buffer_size;
    HashMap<EtherAddress, Vector<HashMap<EtherAddress, struct neighbour_airtime_stats*> > > ether_address_time_map;
};

CLICK_ENDDECLS
#endif	/* COOPERATIVESTATSCIRCULARBUFFER_HH */

