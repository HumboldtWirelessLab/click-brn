#include "nodechannelstats.hh"

CLICK_DECLS
        
NodeChannelStats::NodeChannelStats()
{
    node = EtherAddress();
    _is_fix_endianess = false;
}

NodeChannelStats::NodeChannelStats(EtherAddress &ea)
{
    node = ea;
    _is_fix_endianess = false;
}

void NodeChannelStats::set_stats(struct airtime_stats &new_stats, uint16_t endianess)
{
    memcpy(&stats, &new_stats, sizeof(struct airtime_stats));
    _endianess = endianess;
    _is_fix_endianess = false;
}

struct airtime_stats *NodeChannelStats::get_airtime_stats()
{
    if ( ! _is_fix_endianess )
    {
        //fix it
        _is_fix_endianess = true;
    }
    return &stats;
}

void NodeChannelStats::add_neighbour_stats(EtherAddress &ea, struct neighbour_airtime_stats &stats)
{
    _n_stats.insert(ea, &stats);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(NodeChannelStats)
