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
  return EtherAddress((const unsigned char *) (((uint8_t*)p->addr_anno()) + sizeof(uint32_t)));
}

void
BRNPacketAnno::set_dst_ether_anno(Packet *p, const EtherAddress &a)
{
  memcpy(((uint8_t*)p->addr_anno()) + sizeof(uint32_t), a.data(), 6);
}

String
BRNPacketAnno::udevice_anno(Packet *p)
{
  char device[6];
  memset(device, 0, 6);
  memcpy(device, ((uint8_t*)p->addr_anno()) + sizeof(uint32_t) + 6, 5);
  return String(device);
}

void
BRNPacketAnno::set_udevice_anno(Packet *p, const char *device)
{
  if (strlen(device) <= Packet::ADDR_ANNO_SIZE - sizeof(uint32_t) - 6 - 1)
  {
    void* dst = ((uint8_t*)p->addr_anno()) + sizeof(uint32_t) + 6;
    memset(dst, 0, 5);
    memcpy(dst, device, strlen(device));
  }
  else
    click_chatter("ERROR: Device annotation couldn't be set - was too long. Got '%s'.",
                  device);
}

uint8_t
BRNPacketAnno::tos_anno(Packet *p)
{
  uint8_t* dst = (uint8_t*) ((uint8_t*)(p->addr_anno()) + sizeof(uint32_t) + 6 + 5);
  return (dst[0]);
}

void
BRNPacketAnno::set_tos_anno(Packet *p, uint8_t tos)
{
  uint8_t* dst = (uint8_t*) (((uint8_t*)p->addr_anno()) + sizeof(uint32_t) + 6 + 5);
  dst[0] = tos;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNPacketAnno)

