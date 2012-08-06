/* 
 * File:   nodechannelstats.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 24. Juli 2012, 15:58
 */

#ifndef NODECHANNELSTATS_HH
#define	NODECHANNELSTATS_HH

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#if CLICK_NS 
    #include <click/router.hh>
#endif
#include "elements/brn2/wifi/channelstats.hh"

CLICK_DECLS
        
class NodeChannelStats
{
public:
    NodeChannelStats();
    NodeChannelStats(EtherAddress &ea);
    
    void set_stats(struct airtime_stats &new_stats, uint16_t endianess);
    struct airtime_stats *get_airtime_stats();
    void add_neighbour_stats(EtherAddress &ea, struct neighbour_airtime_stats &stats);
    
    typedef HashMap<EtherAddress, struct neighbour_airtime_stats*>  NeighbourStatsTable;
    typedef NeighbourStatsTable::const_iterator                     NeighbourStatsTableIter;
    
    NeighbourStatsTable     _n_stats;
    EtherAddress            node;
    
private:
    struct airtime_stats    stats;
    uint16_t                _endianess;
    bool                    _is_fix_endianess;
};

CLICK_ENDDECLS
#endif	/* NODECHANNELSTATS_HH */

