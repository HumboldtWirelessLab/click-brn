#ifndef CLICK_BRNPACKETANNO_HH
#define CLICK_BRNPACKETANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

/* next annos ( byte 4-16) overwrites parts of IPv6-annos which are not used if ipv4 is used */

#define DST_ETHER_ANNO_OFFSET    4
#define DST_ETHER_ANNO_SIZE      6

#define SRC_ETHER_ANNO_OFFSET   10
#define SRC_ETHER_ANNO_SIZE      6 

#define UDEVICE_ANNO_OFFSET     10
#define UDEVICE_ANNO_SIZE        6 


/* next annos ( byte 40-43 ) overwrites PERFCTR_ANNO */

#define DEVICENUMBER_ANNO_OFFSET   40
#define DEVICENUMBER_ANNO_SIZE      1

#define TOS_ANNO_OFFSET         41 
#define TOS_ANNO_SIZE            1

#define CHANNEL_ANNO_OFFSET     42
#define CHANNEL_ANNO_SIZE        1

#define OPERATION_ANNO_OFFSET   43
#define OPERATION_ANNO_SIZE      1


/* Operations */

#define OPERATION_SET_CHANNEL_BEFORE_PACKET  1
#define OPERATION_SET_CHANNEL_AFTER_PACKET   2


class BRNPacketAnno : public Element { public:

  BRNPacketAnno();
  ~BRNPacketAnno();

  const char *class_name() const	{ return "BRNPacketAnno"; }

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
