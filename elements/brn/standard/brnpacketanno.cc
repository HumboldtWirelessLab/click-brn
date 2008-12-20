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

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNPacketAnno)

