#ifndef CLICK_BRNPACKETANNO_HH
#define CLICK_BRNPACKETANNO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

/* next annos ( byte 0-4) overwrites IPv64-annos which are not used for MAC-layer and lower */

#define BRN_WIFI_EXTRA_EXTENTION_ANNO_OFFSET    0
#define BRN_WIFI_EXTRA_EXTENTION_ANNO_SIZE      4

#define BRN_WIFI_EXTRA_RX_STATUS_ANNO_OFFSET    4
#define BRN_WIFI_EXTRA_RX_STATUS_ANNO_SIZE     12

/* next annos ( byte 4-16) overwrites parts of IPv6-annos which are not used if ipv4 is used */

#define DST_ETHER_ANNO_OFFSET    4
#define DST_ETHER_ANNO_SIZE      6

#define SRC_ETHER_ANNO_OFFSET   10
#define SRC_ETHER_ANNO_SIZE      6

#define TTL_ANNO_OFFSET         27
#define TTL_ANNO_SIZE            1

/* next annos ( byte 36-37 ) overwrites SEQUENCE_NUMBER_ANNO and IPSEC_SA_DATA_REFERENCE_ANNO  */

#define PULLED_BYTES_ANNO_OFFSET 36
#define PULLED_BYTES_ANNO_SIZE    2

/* next annos ( byte 40-47 ) overwrites PERFCTR_ANNO (Size is 8 Bytes) */

#define ETHERTYPE_ANNO_OFFSET    40
#define ETHERTYPE_ANNO_SIZE       2

#define BRN_VLAN_ANNO_OFFSET     42
#define BRN_VLAN_ANNO_SIZE        2

#define DEVICENUMBER_ANNO_OFFSET 44
#define DEVICENUMBER_ANNO_SIZE    1

#define TOS_ANNO_OFFSET          45
#define TOS_ANNO_SIZE             1

#define CHANNEL_ANNO_OFFSET      46
#define CHANNEL_ANNO_SIZE         1

#define OPERATION_ANNO_OFFSET    47
#define OPERATION_ANNO_SIZE       1


/* Operations */

#define OPERATION_SET_CHANNEL_NONE           0
#define OPERATION_SET_CHANNEL_BEFORE_PACKET  1
#define OPERATION_SET_CHANNEL_AFTER_PACKET   2


class BRNPacketAnno : public Element { public:

  BRNPacketAnno();
  ~BRNPacketAnno();

  const char *class_name() const	{ return "BRNPacketAnno"; }

  static inline void clean_brn_wifi_extra_extention_anno(const Packet *p) {
    memset(((uint8_t*)(p->anno_u8()) + BRN_WIFI_EXTRA_EXTENTION_ANNO_OFFSET),0,BRN_WIFI_EXTRA_EXTENTION_ANNO_SIZE);
  }

  static inline void* get_brn_wifi_extra_extention_anno(const Packet *p) {
    return (void*)&(((uint8_t*)(p->anno_u8()) + BRN_WIFI_EXTRA_EXTENTION_ANNO_OFFSET)[0]);
  }

  static inline void clean_brn_wifi_extra_rx_status_anno(const Packet *p) {
    memset(((uint8_t*)(p->anno_u8()) + BRN_WIFI_EXTRA_RX_STATUS_ANNO_OFFSET),0,BRN_WIFI_EXTRA_RX_STATUS_ANNO_SIZE);
  }

  static inline void* get_brn_wifi_extra_rx_status_anno(const Packet *p) {
    return (void*)&(((uint8_t*)(p->anno_u8()) + BRN_WIFI_EXTRA_RX_STATUS_ANNO_OFFSET)[0]);
  }

  static EtherAddress dst_ether_anno(Packet *p);
  static void set_dst_ether_anno(Packet *p, const EtherAddress &);

  static EtherAddress src_ether_anno(Packet *p);
  static void set_src_ether_anno(Packet *p, const EtherAddress &);

  static void set_src_and_dst_ether_anno(Packet *p, const EtherAddress &, const EtherAddress &);

  static void set_ether_anno(Packet *p, const EtherAddress &, const EtherAddress &, uint16_t);
  static void set_ether_anno(Packet *p, const uint8_t *src, const uint8_t *dst, uint16_t);

  static uint16_t ethertype_anno(Packet *p);
  static void set_ethertype_anno(Packet *p, uint16_t);

  static uint16_t pulled_bytes_anno(Packet *p);
  static void set_pulled_bytes_anno(Packet *p, const uint16_t p_bytes);
  static void inc_pulled_bytes_anno(Packet *p, const uint16_t inc_bytes);
  static void dec_pulled_bytes_anno(Packet *p, const uint16_t dec_bytes);

  static inline uint8_t devicenumber_anno(const Packet *p) {
    return ((uint8_t*)(p->anno_u8()) + DEVICENUMBER_ANNO_OFFSET)[0];
  }

  static inline void set_devicenumber_anno(Packet *p, uint8_t devnum) {
    ((uint8_t*)((p->anno_u8()) + DEVICENUMBER_ANNO_OFFSET))[0] = devnum;
  }

  static uint16_t vlan_anno(const Packet *p);
  static void set_vlan_anno(Packet *, uint16_t);

  static uint8_t tos_anno(Packet *p);
  static void set_tos_anno(Packet *p, uint8_t tos);

  static uint8_t queue_anno(Packet *p);
  static void set_queue_anno(Packet *p, uint8_t tos);

  static void tos_anno(Packet *p, uint8_t *tos, uint8_t *queue);
  static void set_tos_anno(Packet *p, uint8_t tos, uint8_t queue);

  static uint8_t ttl_anno(Packet *p);
  static void set_ttl_anno(Packet *p, uint8_t ttl);

  static uint8_t channel_anno(Packet *p);
  static uint8_t operation_anno(Packet *p);

  static void set_channel_anno(Packet *p, uint8_t channel, uint8_t operation);
  static void set_channel_anno(Packet *p, uint8_t channel);

};

CLICK_ENDDECLS
#endif
