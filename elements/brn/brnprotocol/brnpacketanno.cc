#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brnpacketanno.hh"

CLICK_DECLS

EtherAddress
BRNPacketAnno::dst_ether_anno(Packet *p)
{
  return EtherAddress((const unsigned char *) ((p->anno_u8()) + DST_ETHER_ANNO_OFFSET));
}

void
BRNPacketAnno::set_dst_ether_anno(Packet *p, const EtherAddress &a)
{
  memcpy(((p->anno_u8()) + DST_ETHER_ANNO_OFFSET), a.data(), DST_ETHER_ANNO_SIZE);
}

EtherAddress
BRNPacketAnno::src_ether_anno(Packet *p)
{
  return EtherAddress((const unsigned char *) ((p->anno_u8()) + SRC_ETHER_ANNO_OFFSET));
}

void
BRNPacketAnno::set_src_ether_anno(Packet *p, const EtherAddress &a)
{
  memcpy(((p->anno_u8()) + SRC_ETHER_ANNO_OFFSET), a.data(), SRC_ETHER_ANNO_SIZE);
}

void
BRNPacketAnno::set_src_and_dst_ether_anno(Packet *p, const EtherAddress &src, const EtherAddress &dst) {
  memcpy(((p->anno_u8()) + SRC_ETHER_ANNO_OFFSET), src.data(), SRC_ETHER_ANNO_SIZE);
  memcpy(((p->anno_u8()) + DST_ETHER_ANNO_OFFSET), dst.data(), DST_ETHER_ANNO_SIZE);
}

void
BRNPacketAnno::set_ether_anno(Packet *p, const EtherAddress &src, const EtherAddress &dst, uint16_t type) {
  memcpy(((p->anno_u8()) + SRC_ETHER_ANNO_OFFSET), src.data(), SRC_ETHER_ANNO_SIZE);
  memcpy(((p->anno_u8()) + DST_ETHER_ANNO_OFFSET), dst.data(), DST_ETHER_ANNO_SIZE);
  uint16_t* t = (uint16_t*) ((p->anno_u8()) + ETHERTYPE_ANNO_OFFSET);
  t[0] = type;
}

void
BRNPacketAnno::set_ether_anno(Packet *p, const uint8_t *src, const uint8_t *dst, uint16_t type)
{
  memcpy(((p->anno_u8()) + SRC_ETHER_ANNO_OFFSET), src, SRC_ETHER_ANNO_SIZE);
  memcpy(((p->anno_u8()) + DST_ETHER_ANNO_OFFSET), dst, DST_ETHER_ANNO_SIZE);
  uint16_t* t = (uint16_t*) ((p->anno_u8()) + ETHERTYPE_ANNO_OFFSET);
  t[0] = type;
}

uint16_t
BRNPacketAnno::ethertype_anno(Packet *p) {
  uint16_t* t = (uint16_t*) ((p->anno_u8()) + ETHERTYPE_ANNO_OFFSET);
  return t[0];
}

void
BRNPacketAnno::set_ethertype_anno(Packet *p, uint16_t type) {
  uint16_t* t = (uint16_t*) ((p->anno_u8()) + ETHERTYPE_ANNO_OFFSET);
  t[0] = type;
}

uint16_t
BRNPacketAnno::pulled_bytes_anno(Packet *p)
{
  uint16_t p_bytes;
  memcpy(&p_bytes, ((uint8_t*)p->anno_u8()) + PULLED_BYTES_ANNO_OFFSET, PULLED_BYTES_ANNO_SIZE);
  return p_bytes;
}

void
BRNPacketAnno::set_pulled_bytes_anno(Packet *p, const uint16_t p_bytes)
{
  memcpy(((uint8_t*)p->anno_u8()) + PULLED_BYTES_ANNO_OFFSET, &p_bytes, PULLED_BYTES_ANNO_SIZE);
}

void
BRNPacketAnno::inc_pulled_bytes_anno(Packet *p, const uint16_t inc_bytes)
{
  uint16_t *p_bytes;

  memcpy(&p_bytes, ((uint8_t*)p->anno_u8()) + PULLED_BYTES_ANNO_OFFSET, PULLED_BYTES_ANNO_SIZE);
  p_bytes += inc_bytes;
  memcpy(((uint8_t*)p->anno_u8()) + PULLED_BYTES_ANNO_OFFSET, &p_bytes, PULLED_BYTES_ANNO_SIZE);
}

void
BRNPacketAnno::dec_pulled_bytes_anno(Packet *p, const uint16_t dec_bytes)
{
  uint16_t *p_bytes;

  memcpy(&p_bytes, ((uint8_t*)p->anno_u8()) + PULLED_BYTES_ANNO_OFFSET, PULLED_BYTES_ANNO_SIZE);
  p_bytes -= dec_bytes;
  memcpy(((uint8_t*)p->anno_u8()) + PULLED_BYTES_ANNO_OFFSET, &p_bytes, PULLED_BYTES_ANNO_SIZE);
}

uint16_t
BRNPacketAnno::vlan_anno(const Packet *p)
{
  uint16_t* dst = ((uint16_t*)(p->anno_u8()) + BRN_VLAN_ANNO_OFFSET);
  return (dst[0]);
}

void
BRNPacketAnno::set_vlan_anno(Packet *p, uint16_t vlan)
{
  uint16_t* dst = (uint16_t*) ((p->anno_u8()) + BRN_VLAN_ANNO_OFFSET);
  dst[0] = vlan;
}

uint8_t
BRNPacketAnno::tos_anno(Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) + TOS_ANNO_OFFSET);
  return (dst[0] & 0x0F);
}

void
BRNPacketAnno::set_tos_anno(Packet *p, uint8_t tos)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) + TOS_ANNO_OFFSET);
  dst[0] = (dst[0] & 0xF0) | tos;
}

uint8_t
BRNPacketAnno::queue_anno(Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) + TOS_ANNO_OFFSET);
  return ((dst[0] & 0xF0) >> 4);
}

void
BRNPacketAnno::set_queue_anno(Packet *p, uint8_t queue)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) + TOS_ANNO_OFFSET);
  dst[0] = (dst[0] & 0x0F) | (queue << 4);
}

void
BRNPacketAnno::tos_anno(Packet *p, uint8_t *tos, uint8_t *queue)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) + TOS_ANNO_OFFSET);
  *tos = (uint8_t)(dst[0] & 0x0F);
  *queue = (uint8_t)((dst[0] & 0xF0) >> 4);
}

void
BRNPacketAnno::set_tos_anno(Packet *p, uint8_t tos, uint8_t queue)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) + TOS_ANNO_OFFSET);
  dst[0] = tos | (queue << 4);
}

uint8_t
BRNPacketAnno::ttl_anno(Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) + TTL_ANNO_OFFSET);
  return (dst[0]);
}

void
BRNPacketAnno::set_ttl_anno(Packet *p, uint8_t ttl)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) + TTL_ANNO_OFFSET);
  dst[0] = ttl;
}

uint8_t
BRNPacketAnno::channel_anno(Packet *p)
{
  uint8_t* channel = ((uint8_t*)(p->anno_u8()) + CHANNEL_ANNO_OFFSET);
  return (channel[0]);
}

uint8_t
BRNPacketAnno::operation_anno(Packet *p)
{
  uint8_t* op = ((uint8_t*)(p->anno_u8()) + OPERATION_ANNO_OFFSET);
  return (op[0]);
}

void
BRNPacketAnno::set_channel_anno(Packet *p, uint8_t channel, uint8_t operation)
{
  uint8_t* ch = (uint8_t*) ((p->anno_u8()) + CHANNEL_ANNO_OFFSET);
  uint8_t* op = (uint8_t*) ((p->anno_u8()) + OPERATION_ANNO_OFFSET);
  ch[0] = channel;
  op[0] = operation;
}

void
BRNPacketAnno::set_channel_anno(Packet *p, uint8_t channel)
{
  set_channel_anno(p, channel, OPERATION_SET_CHANNEL_NONE);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNPacketAnno)

