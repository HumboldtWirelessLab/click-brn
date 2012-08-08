/* 
 * File:   statscircularbuffer.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 20. Juli 2012, 14:01
 */

#ifndef STATSCIRCULARBUFFER_HH
#define	STATSCIRCULARBUFFER_HH
#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/hashmap.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "elements/brn2/brnelement.hh"
#include "packetparameter.hh"
#include "packetlossstatistics.hh"
#include "elements/brn2/wifi/packetlossinformation/packetlossinformation.hh"

CLICK_DECLS

class StatsCircularBuffer
{

public:
    StatsCircularBuffer(const uint16_t);    
    virtual ~StatsCircularBuffer();
    void insert_values(PacketParameter &, PacketLossInformation &);
    Vector<PacketLossStatistics> get_values(EtherAddress &, uint16_t);
    void set_buffer_size(uint16_t size);
    uint16_t get_buffer_size();
    Vector<PacketLossStatistics> get_all_values(EtherAddress &);
    //std::list<PacketLossStatistics> get_all_values(EtherAddress &);
    Vector<EtherAddress> get_stored_addresses();
    
private:
    StatsCircularBuffer();
    uint16_t buffer_size;

    //std::map<EtherAddress, Vector<PacketLossStatistics> > ether_address_time_map;
    HashMap<EtherAddress, Vector<PacketLossStatistics> > ether_address_time_map;
};

CLICK_ENDDECLS
#endif	/* STATSCIRCULARBUFFER_HH */
        
/*
    virtual bool add_etheraddress(const EtherAddress &);
    std::map<EtherAddress, Vector<AbstractPacketLossStatistics *> > get_ether_address_time_map();
*/
