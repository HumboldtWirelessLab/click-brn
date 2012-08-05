#include "cooperativestatscircularbuffer.hh"

CLICK_DECLS

CooperativeStatsCircularBuffer::CooperativeStatsCircularBuffer(const uint32_t start_size)
{
    airtime_stats_history = new struct neighbour_airtime_stats[start_size];
}

CooperativeStatsCircularBuffer::~CooperativeStatsCircularBuffer()
{
    delete[] airtime_stats_history;
}

void CooperativeStatsCircularBuffer::insert_values(PacketParameter &, PacketLossInformation &) {
    
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossInformation)
ELEMENT_REQUIRES(PacketParameter)
ELEMENT_PROVIDES(CooperativeStatsCircularBuffer)
        
        /*
         
         AbstractCircularBuffer::AbstractCircularBuffer()
{
    ether_address_time_map = std::map<EtherAddress, Vector<AbstractPacketLossStatistics *> >();
    buffer_size = 200;
}

AbstractCircularBuffer::AbstractCircularBuffer(const uint16_t size)
{
    ether_address_time_map = std::map<EtherAddress, Vector<AbstractPacketLossStatistics *> >();
    buffer_size = size;
}

AbstractCircularBuffer::~AbstractCircularBuffer()
{
}

bool AbstractCircularBuffer::add_etheraddress(const EtherAddress &ether_address)
{
    Vector<AbstractPacketLossStatistics *> temp_vector = Vector<AbstractPacketLossStatistics *>();
    
    if (ether_address_time_map.find(ether_address) == ether_address_time_map.end())
    {
        ether_address_time_map.insert(std::make_pair(ether_address, temp_vector));
        return true;
    }
    
    return false;
}

Vector<AbstractPacketLossStatistics *> AbstractCircularBuffer::get_all_values(EtherAddress &ether_address)
{
    return ether_address_time_map.at(ether_address);
}

const uint16_t AbstractCircularBuffer::get_buffer_size()
{
    
}

void AbstractCircularBuffer::set_buffer_size(const uint16_t size)
{
    buffer_size = size;
}

std::map<EtherAddress, Vector<AbstractPacketLossStatistics *> > AbstractCircularBuffer::get_ether_address_time_map()
{
    return ether_address_time_map;
}

         
         */