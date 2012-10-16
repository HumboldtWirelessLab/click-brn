/* 
 * File:   packetparameter.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 22. Juli 2012, 20:58
 */

#ifndef PACKETPARAMETER_HH
#define	PACKETPARAMETER_HH

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include "elements/brn2/brn2.h"
#if CLICK_NS 
    #include <click/router.hh>
#endif

CLICK_DECLS

class PacketParameter
{
public:
    PacketParameter ();
    virtual ~PacketParameter ();
    
    const EtherAddress *get_own_address ();
    const EtherAddress *get_src_address ();
    EtherAddress get_non_const_src_address ();
    const EtherAddress *get_dst_address ();
    uint8_t get_packet_type ();
    void put_params_ (const EtherAddress &, const EtherAddress &, const EtherAddress &, const uint8_t, const uint8_t);
    bool is_broadcast_or_self ();
    uint8_t get_rate ();
    
private:
    EtherAddress own_address;
    EtherAddress src_address;
    EtherAddress dst_address;
    uint8_t packet_type;
    uint8_t rate;
};

CLICK_ENDDECLS        
#endif	/* PACKETPARAMETER_HH */

