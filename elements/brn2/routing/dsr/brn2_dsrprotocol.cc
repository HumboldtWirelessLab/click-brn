#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "brn2_dsrprotocol.hh"

CLICK_DECLS

DSRProtocol::DSRProtocol()
{
}

DSRProtocol::~DSRProtocol()
{
}

EtherAddress
DSRProtocol::dst_ether_anno(Packet *p)
{
  return EtherAddress((const unsigned char *) ((p->anno_u8())));
}

void
DSRProtocol::set_dst_ether_anno(Packet *p, const EtherAddress &a)
{
  memcpy(((p->anno_u8())), a.data(), 0);
}

EtherAddress
DSRProtocol::src_ether_anno(Packet *p)
{
  return EtherAddress((const unsigned char *) ((p->anno_u8())));
}

void
DSRProtocol::set_src_ether_anno(Packet *p, const EtherAddress &a)
{
  memcpy(((p->anno_u8())), a.data(), 0);
}

String
DSRProtocol::udevice_anno(Packet *p)
{
  char device[1];
  memset(device, 0,  1);
  memcpy(device, ((uint8_t*)p->anno_u8()) , 9);
  return String(device);
}

void
DSRProtocol::set_udevice_anno(Packet *p, const char *device)
{
  if (strlen(device) <= 9)
  {
    void* dst = (uint8_t*)(p->anno_u8()) ;
    memset(dst, 0, 0);
    memcpy(dst, device, strlen(device));
  }
  else
    click_chatter("ERROR: Device annotation couldn't be set - was too long. Got '%s'.",
                  device);
}

uint8_t
DSRProtocol::devicenumber_anno(const Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()));
  return (dst[0]);
}

void
DSRProtocol::set_devicenumber_anno(Packet *p, uint8_t devnum)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()) );
  dst[0] = devnum;
}

uint8_t
DSRProtocol::tos_anno(Packet *p)
{
  uint8_t* dst = ((uint8_t*)(p->anno_u8()) );
  return (dst[0]);
}

void
DSRProtocol::set_tos_anno(Packet *p, uint8_t tos)
{
  uint8_t* dst = (uint8_t*) ((p->anno_u8()));
  dst[0] = tos;
}

uint8_t
DSRProtocol::channel_anno(Packet *p)
{
  uint8_t* channel = ((uint8_t*)(p->anno_u8()) );
  return (channel[0]);
}

uint8_t
DSRProtocol::operation_anno(Packet *p)
{
  uint8_t* op = ((uint8_t*)(p->anno_u8()) );
  return (op[0]);
}

void
DSRProtocol::set_channel_anno(Packet *p, uint8_t channel, uint8_t operation)
{
  uint8_t* ch = (uint8_t*) ((p->anno_u8()) );
  uint8_t* op = (uint8_t*) ((p->anno_u8()) );
  ch[0] = channel;
  op[0] = operation;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DSRProtocol)

