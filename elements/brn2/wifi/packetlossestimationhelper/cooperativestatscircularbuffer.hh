/* 
 * File:   cooperativestatscircularbuffer.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 24. Juli 2012, 16:09
 */

#ifndef COOPERATIVESTATSCIRCULARBUFFER_HH
#define	COOPERATIVESTATSCIRCULARBUFFER_HH

#include <map>
#include <click/config.h>
#include <click/straccum.hh>
#include <click/error.hh>
#include <click/args.hh>
#if CLICK_NS 
    #include <click/router.hh>
#endif
#include "elements/brn2/wifi/channelstats.hh"
#include "elements/brn2/wifi/packetlossestimationhelper/packetparameter.hh"
#include "elements/brn2/wifi/packetlossinformation/packetlossinformation.hh"

CLICK_DECLS

class CooperativeStatsCircularBuffer
{
public:
    CooperativeStatsCircularBuffer(const uint32_t start_size);
    virtual ~CooperativeStatsCircularBuffer();
    void insert_values(PacketParameter &, PacketLossInformation &);
//    virtual Vector<neighbour_airtime_stats> get_values(EtherAddress &, uint16_t);
//    virtual Vector<neighbour_airtime_stats> get_all_values(EtherAddress &);
    
private:
    CooperativeStatsCircularBuffer();
    uint16_t buffer_size;
    std::map<EtherAddress, Vector<PacketLossInformation> > ether_address_time_map;

    struct neighbour_airtime_stats* airtime_stats_history;
    uint32_t size;
    uint32_t start_elem;
    uint32_t counter;
};

CLICK_ENDDECLS
#endif	/* COOPERATIVESTATSCIRCULARBUFFER_HH */

