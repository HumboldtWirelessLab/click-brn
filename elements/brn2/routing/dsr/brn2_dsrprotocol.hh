#ifndef CLICK_BRN2DSRPROTOCOL_HH
#define CLICK_BRN2DSRPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS


class DSRProtocol : public Element { public:

  DSRProtocol();
  ~DSRProtocol();

  const char *class_name() const	{ return "DSRProtocol"; }

  static EtherAddress dst_ether_anno(Packet *p);
  static void set_dst_ether_anno(Packet *p, const EtherAddress &);

  static EtherAddress src_ether_anno(Packet *p);
  static void set_src_ether_anno(Packet *p, const EtherAddress &);

  static String udevice_anno(Packet *p);
  static void set_udevice_anno(Packet *p, const char *device);

  static uint8_t devicenumber_anno(const Packet *p);
  static void set_devicenumber_anno(Packet *, uint8_t);

  static uint8_t tos_anno(Packet *p);
  static void set_tos_anno(Packet *p, uint8_t tos);

  static uint8_t channel_anno(Packet *p);
  static uint8_t operation_anno(Packet *p);

  static void set_channel_anno(Packet *p, uint8_t channel, uint8_t operation);

};

CLICK_ENDDECLS
#endif
