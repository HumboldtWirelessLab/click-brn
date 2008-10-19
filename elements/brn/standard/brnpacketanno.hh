#ifndef CLICK_BRNPACKETANNO_HH
#define CLICK_BRNPACKETANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

class BRNPacketAnno : public Element { public:

  BRNPacketAnno();
  ~BRNPacketAnno();

  const char *class_name() const	{ return "BRNPacketAnno"; }

  String udevice_anno(Packet *p) const;
  void set_udevice_anno(Packet *p, const char *device);

  uint8_t tos_anno(Packet *p) const;
  void set_tos_anno(Packet *p, uint8_t tos);

  EtherAddress dst_ether_anno(Packet *p) const;
  void set_dst_ether_anno(Packet *p, const EtherAddress &);

};

CLICK_ENDDECLS
#endif
