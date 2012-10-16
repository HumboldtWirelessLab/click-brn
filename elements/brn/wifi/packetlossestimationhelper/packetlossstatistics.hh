/* 
 * File:   packetlossstatistics.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 23. Juli 2012, 13:39
 */

#ifndef PACKETLOSSSTATISTICS_HH
#define	PACKETLOSSSTATISTICS_HH

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#if CLICK_NS 
    #include <click/router.hh>
#endif
#include "elements/brn/brnelement.hh"

CLICK_DECLS

class PacketLossStatistics
{
public:
    PacketLossStatistics();
    virtual ~PacketLossStatistics();
    
    int get_time_stamp();
    void set_time_stamp(int time);
    uint8_t get_inrange_probability();
    void set_inrange_probability(uint8_t inrange);
    uint8_t get_hidden_node_probability();
    void set_hidden_node_probability(uint8_t hidden_node);
    uint8_t get_weak_signal_probability();
    void set_weak_signal_probability(uint8_t weak_signal);
    uint8_t get_non_wifi_probability();
    void set_non_wifi_probability(uint8_t non_wifi);
    
private:
    int timestamp;
    uint8_t inrange_prob;
    uint8_t hidden_node_prob;
    uint8_t weak_signal_prob;
    uint8_t non_wifi_prob;
};

CLICK_ENDDECLS
#endif	/* PACKETLOSSSTATISTICS_HH */

