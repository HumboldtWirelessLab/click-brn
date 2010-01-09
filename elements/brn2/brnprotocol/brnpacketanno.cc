#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brnpacketanno.hh"

CLICK_DECLS

BRNPacketAnno::BRNPacketAnno()
{
}

BRNPacketAnno::~BRNPacketAnno()
{
}

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
  t[0] = htons(type);
}

uint16_t
BRNPacketAnno::ethertype_anno(Packet *p) {
  uint16_t* t = (uint16_t*) ((p->anno_u8()) + ETHERTYPE_ANNO_OFFSET);
  return ntohs(t[0]); 
}

void
BRNPacketAnno::set_ethertype_anno(Packet *p, uint16_t type) {
  uint16_t* t = (uint16_t*) ((p->anno_u8()) + ETHERTYPE_ANNO_OFFSET);
  t[0] = htons(type);
}

String
BRNPacketAnno::udevice_anno(Packet *p)
{
  char device[UDEVICE_ANNO_SIZE + 1];
  memset(device, 0, UDEVICE_ANNO_SIZE + 1);
  memcpy(device, ((uint8_t*)p->anno_u8()) + UDEVICE_ANNO_OFFSET, UDEVICE_ANNO_SIZE);
  return String(device);
}

void
BRNPacketAnno::set_udevice_anno(Packet *p, const char *device)
{
  if (strlen(device) <= UDEVICE_ANNO_SIZE)
  {
    void* dst = (uint8_t*)(p->anno_u8()) + UDEVICE_ANNO_OFFSET;
    memset(dst, 0, UDEVICE_ANNO_SIZE);
    memcpy(dst, device, strlen(device));
  }
  else
    click_chatter("ERROR: Device annotation couldn't be set - was too long. Got '%s'.",
                  device);
}

uint8_t
BRNPacketAnno::devicenumber_anno(const Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) + DEVICENUMBER_ANNO_OFFSET);
  return (dst[0]);
}

void
BRNPacketAnno::set_devicenumber_anno(Packet *p, uint8_t devnum)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) + DEVICENUMBER_ANNO_OFFSET);
  dst[0] = devnum;
}

uint16_t
BRNPacketAnno::vlan_anno(const Packet *p)
{
  uint16_t* dst = ((uint16_t*)(p->anno_u8()) + VLAN_ANNO_OFFSET);
  return (dst[0]);
}

void
BRNPacketAnno::set_vlan_anno(Packet *p, uint16_t vlan)
{
  uint16_t* dst = (uint16_t*) ((p->anno_u8()) + VLAN_ANNO_OFFSET);
  dst[0] = vlan;
}

uint8_t
BRNPacketAnno::tos_anno(Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) + TOS_ANNO_OFFSET);
  return (dst[0]);
}

void
BRNPacketAnno::set_tos_anno(Packet *p, uint8_t tos)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) + TOS_ANNO_OFFSET);
  dst[0] = tos;
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
EXPORT_ELEMENT(BRNPacketAnno)

