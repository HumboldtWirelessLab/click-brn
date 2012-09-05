#include "packetparameter.hh"
CLICK_DECLS

PacketParameter::PacketParameter ()
{
    packet_type = 0;
    rate = 0;
    own_address = brn_etheraddress_broadcast;
    src_address = brn_etheraddress_broadcast;
    dst_address = brn_etheraddress_broadcast;
}

PacketParameter::~PacketParameter ()
{
}

const EtherAddress *PacketParameter::get_own_address ()
{
    return &own_address;
}

const EtherAddress *PacketParameter::get_src_address ()
{
    return &src_address;
}

EtherAddress PacketParameter::get_non_const_src_address ()
{
    return src_address;
}

const EtherAddress *PacketParameter::get_dst_address ()
{
    return &dst_address;
}

uint8_t PacketParameter::get_packet_type ()
{
    return packet_type;
}

uint8_t PacketParameter::get_rate ()
{
    return rate;
}

void PacketParameter::put_params_ (const EtherAddress &own, const EtherAddress &src, const EtherAddress &dst, const uint8_t p_type, const uint8_t rate_val)
{
    own_address = own;
    src_address = src;
    dst_address = dst;
    packet_type = p_type;
    rate = rate_val;
}

bool PacketParameter::is_broadcast_or_self ()
{
    return (*get_src_address () == brn_etheraddress_broadcast || *get_src_address () == *get_own_address ());
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(PacketParameter)
