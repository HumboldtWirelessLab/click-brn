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
        pls_temp_vector.push_front (pls);
		ether_address_time_map.erase (ea);

        if (pls_temp_vector.size () > buffer_size)
        {
            pls_temp_vector.pop_back ();
        }

        ether_address_time_map.insert (ea, pls_temp_vector);
    }
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
        click_chatter ("StatsCircularBuffer: no packet-loss data collected for longer period");
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
        ether_address_time_map.insert (ea, Vector<PacketLossStatistics> ());
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
