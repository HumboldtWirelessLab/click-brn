#include "packetlossstatistics.hh"
CLICK_DECLS
    
PacketLossStatistics::PacketLossStatistics()
{
    timestamp = 0;
    inrange_prob = 0;
    hidden_node_prob = 0;
    weak_signal_prob = 0;
    non_wifi_prob = 0;
}

PacketLossStatistics::~PacketLossStatistics()
{
}

int PacketLossStatistics::get_time_stamp()
{
    return timestamp;
}

void PacketLossStatistics::set_time_stamp(int time)
{
    timestamp = time;
}

uint8_t PacketLossStatistics::get_inrange_probability()
{
    return inrange_prob;
}

void PacketLossStatistics::set_inrange_probability(uint8_t inrange)
{
    inrange_prob = inrange;
}

uint8_t PacketLossStatistics::get_hidden_node_probability()
{
    return hidden_node_prob;
}

void PacketLossStatistics::set_hidden_node_probability(uint8_t hidden_node)
{
    hidden_node_prob = hidden_node;
}

uint8_t PacketLossStatistics::get_weak_signal_probability()
{
    return weak_signal_prob;
}

void PacketLossStatistics::set_weak_signal_probability(uint8_t weak_signal)
{
    weak_signal_prob = weak_signal;
}

uint8_t PacketLossStatistics::get_non_wifi_probability()
{
    return non_wifi_prob;
}

void PacketLossStatistics::set_non_wifi_probability(uint8_t non_wifi)
{
    non_wifi_prob = non_wifi;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(PacketLossStatistics)
