#include "cooperativestatscircularbuffer.hh"

CLICK_DECLS

CooperativeStatsCircularBuffer::CooperativeStatsCircularBuffer(const uint32_t start_size)
{
    buffer_size = start_size;
}

CooperativeStatsCircularBuffer::~CooperativeStatsCircularBuffer()
{
}

void CooperativeStatsCircularBuffer::insert_values(NodeChannelStats &ncst)
{
    if(&ncst == NULL)
    {
        return;
    }

    EtherAddress *ea = ncst.get_address();
    HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map = ncst.get_neighbour_stats_table();
    Vector<HashMap<EtherAddress, struct neighbour_airtime_stats*> > nats_temp_vector;

    if(ether_address_time_map.find(*ea).empty())
    {
        Vector<HashMap<EtherAddress, struct neighbour_airtime_stats*> > new_nats_temp_vector;
        new_nats_temp_vector.push_front(nats_map);
        ether_address_time_map.insert(*ea, new_nats_temp_vector);
    } else
    {
        nats_temp_vector.push_front(nats_map);
        ether_address_time_map.erase(*ea);

        if(nats_temp_vector.size() > buffer_size)
        {
            nats_temp_vector.pop_back();
        }

        ether_address_time_map.insert(*ea, nats_temp_vector);
    }
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossInformation)
ELEMENT_REQUIRES(PacketParameter)
ELEMENT_PROVIDES(CooperativeStatsCircularBuffer)
