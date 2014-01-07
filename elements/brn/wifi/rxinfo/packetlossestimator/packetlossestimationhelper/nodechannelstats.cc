#include "nodechannelstats.hh"

CLICK_DECLS

NodeChannelStats::NodeChannelStats()
{
    node = EtherAddress();
    _is_fix_endianess = false;
    _endianess = ENDIANESS_TEST
    ;
}

NodeChannelStats::NodeChannelStats(EtherAddress &ea)
{
    node = ea;
    _is_fix_endianess = false;
    _endianess = ENDIANESS_TEST
    ;
}

NodeChannelStats::~NodeChannelStats()
{
}

void NodeChannelStats::set_stats(struct airtime_stats &new_stats, uint16_t endianess)
{
    memcpy(&stats, &new_stats, sizeof(struct airtime_stats));
    _endianess = endianess;
    _is_fix_endianess = false;
}

struct airtime_stats *NodeChannelStats::get_airtime_stats()
{
    if(!_is_fix_endianess)
    {
        if(_endianess == 1234)
        {
            _is_fix_endianess = true;
        } else
        {
            //TODO: fix endianess
        }
    }
    return &stats;
}

void NodeChannelStats::add_neighbour_stats(EtherAddress &ea, struct neighbour_airtime_stats &stats)
{
    _n_stats.insert(ea, &stats);
}

EtherAddress *NodeChannelStats::get_address()
{
    return &node;
}

HashMap<EtherAddress, struct neighbour_airtime_stats*> NodeChannelStats::get_neighbour_stats_table()
{
    return _n_stats;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(NodeChannelStats)
