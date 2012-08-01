#include "statscircularbuffer.hh"

CLICK_DECLS

StatsCircularBuffer::StatsCircularBuffer(const uint16_t initsize)
{
    set_buffer_size(initsize);
}

StatsCircularBuffer::~StatsCircularBuffer()
{    
}

void StatsCircularBuffer::insert_values(PacketParameter &packet_parameter, PacketLossInformation &pli)
{
    EtherAddress ea = *packet_parameter.get_src_address();
    const uint8_t hn_fraction = pli.graph_get(ea)->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction();
    const uint8_t ir_fraction = pli.graph_get(ea)->reason_get(PacketLossReason::IN_RANGE)->getFraction();
    const uint8_t nw_fraction = pli.graph_get(ea)->reason_get(PacketLossReason::NON_WIFI)->getFraction();
    const uint8_t ws_fraction = pli.graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction();
    
    PacketLossStatistics pls;
    pls.set_hidden_node_probability(hn_fraction);
    pls.set_inrange_probability(ir_fraction);
    pls.set_non_wifi_probability(nw_fraction);
    pls.set_weak_signal_probability(ws_fraction);

    get_all_values(ea).push_front(pls);

    if (get_all_values(ea).size() > get_buffer_size())
    {
        get_all_values(ea).pop_back();
    }
}

Vector<PacketLossStatistics> StatsCircularBuffer::get_values(EtherAddress &ea, uint16_t amount)
{
    Vector<PacketLossStatistics> stored_pls = get_all_values(ea);
    if (amount > get_buffer_size())
    {
        amount = get_buffer_size();
    }
    
    Vector<PacketLossStatistics> pls;
    
    for (int i = 0; i < amount; i++)
    {        
        pls.push_front(stored_pls.at(i));
    }
    
    return pls;
}

void StatsCircularBuffer::set_buffer_size(uint16_t size)
{
    buffer_size = size;
}

uint16_t StatsCircularBuffer::get_buffer_size()
{
    return buffer_size;
}

Vector<PacketLossStatistics> StatsCircularBuffer::get_all_values(EtherAddress &ea)
{
    if (ether_address_time_map.find (ea) == ether_address_time_map.end ())
    {
        Vector<PacketLossStatistics> temp_vector;
        ether_address_time_map.insert (std::make_pair (ea, temp_vector));
    }
    
    return ether_address_time_map.at(ea);
}

Vector<EtherAddress> StatsCircularBuffer::get_stored_addressess()
{
    Vector<EtherAddress> ether_addresses;
    std::map<EtherAddress, Vector<PacketLossStatistics> >::iterator iter;

    for (iter = ether_address_time_map.begin (); iter != ether_address_time_map.end (); iter++)
    {
        click_chatter (iter->first.unparse ().data ());
        ether_addresses.push_back (iter->first);
    }
    return ether_addresses;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketLossInformation)
ELEMENT_REQUIRES(PacketParameter)
ELEMENT_REQUIRES(PacketLossStatistics)
ELEMENT_PROVIDES(StatsCircularBuffer)
        
/*
        uint32_t size;
        uint32_t start_elem;
        uint32_t counter;

        HashMap<EtherAddress, struct packetloss_statistics_old>* time_buffer;

    public:
        StatsCircularBuffer_old(uint32_t start_size) {

            start_elem = 0;
            counter = 0;
            size = start_size;
            time_buffer = new HashMap<EtherAddress, struct packetloss_statistics_old>[start_size];
        }

        ~StatsCircularBuffer_old() {

            delete[] time_buffer;
            start_elem = 0;
            counter = 0;
            size = 0;
        }

        inline uint32_t get_size() {
            return size;
        }

        inline bool is_full () {
            return size == counter;
        }

        inline bool is_empty () {
            return counter == 0;
        }

        void put_data_in (HashMap<EtherAddress, struct packetloss_statistics_old> *data) {

            uint32_t last_elem = (start_elem + counter) % size;
            time_buffer[last_elem] = *data;

            if (size == counter) {
                start_elem = (start_elem + 1) % size;   // if circular buffer filled completely override
            } else {
                start_elem++;
            }
        }

        bool pull_data (HashMap<EtherAddress, struct packetloss_statistics_old> *data) {

            *data = time_buffer[start_elem];
            start_elem = (start_elem + 1) % size;
            counter--;
            return true;
        }

        uint32_t read_data (HashMap<EtherAddress, struct packetloss_statistics_old> *data, uint32_t entries) {

            if (entries > size) {
                return 0;
            }

            HashMap<EtherAddress, struct packetloss_statistics_old> temp_data[entries];
            uint32_t overflow_counter = size;
            uint32_t i = 0;

            for (i; i < entries; i++) {

                if (start_elem - i < 0) {

                    if (NULL == &time_buffer[overflow_counter]) {

                        break;
                    }

                    temp_data[i] = time_buffer[overflow_counter--];

                } else {
                    temp_data[i] = time_buffer[start_elem - i];
                }
            }

            *data = *temp_data;
            return i;
        }
         *
         *
         *
         *
         *
         *

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

*/