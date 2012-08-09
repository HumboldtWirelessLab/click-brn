#include "statscircularbuffer.hh"

CLICK_DECLS

StatsCircularBuffer::StatsCircularBuffer (const uint16_t initsize)
{
    set_buffer_size (initsize);
}

StatsCircularBuffer::~StatsCircularBuffer ()
{    
}

void StatsCircularBuffer::insert_values (PacketParameter &packet_parameter, PacketLossInformation &pli)
{
    if (&packet_parameter == NULL || &pli == NULL)
    {
        click_chatter ("%s:%d: PacketParameter and/or PacketLossInformation is NULL", __FILE__, __LINE__);
        return;
    }
    
    EtherAddress                ea = *packet_parameter.get_src_address();
    PacketLossInformation_Graph *pli_graph = pli.graph_get(ea);

    if (pli_graph == NULL)
    {
        click_chatter ("%s:%d: Could not find PacketLossInformation_Graph for %s", __FILE__, __LINE__, ea.unparse ().c_str ());
        return;
    }

    const uint8_t hn_fraction = pli_graph->reason_get (PacketLossReason::HIDDEN_NODE)->getFraction ();
    const uint8_t ir_fraction = pli_graph->reason_get (PacketLossReason::IN_RANGE)->getFraction ();
    const uint8_t nw_fraction = pli_graph->reason_get (PacketLossReason::NON_WIFI)->getFraction ();
    const uint8_t ws_fraction = pli_graph->reason_get (PacketLossReason::WEAK_SIGNAL)->getFraction ();

    PacketLossStatistics pls;
    
    pls.set_hidden_node_probability (hn_fraction);
    pls.set_inrange_probability (ir_fraction);
    pls.set_non_wifi_probability (nw_fraction);
    pls.set_weak_signal_probability (ws_fraction);

    Vector<PacketLossStatistics> pls_temp_vector = ether_address_time_map.find (ea);

    if (pls_temp_vector.empty ())
    {
        Vector<PacketLossStatistics> new_pls_temp_vector;
        new_pls_temp_vector.push_front (pls);
        ether_address_time_map.insert (ea, new_pls_temp_vector);
    } else
    {
        //Vector<PacketLossStatistics> pls_temp_vector = ether_address_time_map.find (ea);
        
        //click_chatter ("pls_temp_list-size before push: %d", pls_temp_list.size ());
        
        //int size = get_all_values(ea).size ();
        //for (Vector<PacketLossStatistics>::iterator iter = get_all_values(ea).; iter != get_all_values(ea).end(); iter++)
//        for (int i = 0; i <= size; i++)
//        {
//            click_chatter ("in for: %d", i);
//            temp_vector.push_back (get_all_values(ea).unchecked_at (i));
//        }
        //get_all_values(ea).push_front(pls);

        pls_temp_vector.push_front (pls);
		ether_address_time_map.erase (ea);

        if (pls_temp_vector.size () > buffer_size)
        {
            pls_temp_vector.pop_back ();
        }

        ether_address_time_map.insert (ea, pls_temp_vector);
        
        //click_chatter ("size for %s in map: %d", ea.unparse ().data () , ether_address_time_map.at (ea).size ());
        
/*        if (get_all_values(ea).size() > get_buffer_size())
        {
            get_all_values(ea).pop_back();
        }
 */   
    }
    //click_chatter ("map-size: %d", ether_address_time_map.size());
}

Vector<PacketLossStatistics> StatsCircularBuffer::get_values (EtherAddress &ea, uint16_t amount)
{
    Vector<PacketLossStatistics> pls;
    Vector<PacketLossStatistics> stored_pls = ether_address_time_map.find (ea);

    if (!stored_pls.empty ())
    {
        if (amount > get_buffer_size ())
        {
            amount = get_buffer_size ();
        } else if (amount > stored_pls.size ())
        {
            amount = stored_pls.size ();
        }

        int                                     i = 0;
        Vector<PacketLossStatistics>::iterator  list_end = stored_pls.end ();

        for (Vector<PacketLossStatistics>::iterator list_iter = stored_pls.begin (); list_iter != list_end && i <= amount; ++list_iter)
        {
            ++i;
            pls.push_back (*list_iter);
        }
        //click_chatter ("amount/iterations: %d/%d", amount, i);
    } else
    {
        click_chatter("StatsCircularBuffer: no packet-loss data collected for longer period");
    }

    return pls;
}

void StatsCircularBuffer::set_buffer_size (uint16_t size)
{
    buffer_size = size;
}

uint16_t StatsCircularBuffer::get_buffer_size ()
{
    return buffer_size;
}

Vector<PacketLossStatistics> StatsCircularBuffer::get_all_values (EtherAddress &ea)
{
    if (ether_address_time_map.find (ea).empty ())
    {
        //Vector<PacketLossStatistics> pls_temp_vector;;
        ether_address_time_map.insert (ea, Vector<PacketLossStatistics>());
    }

    return ether_address_time_map.find (ea);
}

Vector<EtherAddress> StatsCircularBuffer::get_stored_addresses ()
{
    Vector<EtherAddress>                                            ether_addresses;
    HashMap<EtherAddress,Vector<PacketLossStatistics> >::iterator   end_of_map = ether_address_time_map.end ();

    for (HashMap<EtherAddress, Vector<PacketLossStatistics> >::iterator iter = ether_address_time_map.begin (); iter != end_of_map; iter++)
    {
        ether_addresses.push_back (iter.key ());
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
