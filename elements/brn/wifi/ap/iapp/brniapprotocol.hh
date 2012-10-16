#ifndef CLICK_BRNIAPPROTOCOL_HH
#define CLICK_BRNIAPPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

struct click_brn_iapp_ho {
  uint8_t       addr_sta[6];  ///< Address of the roaming station
  uint8_t       addr_mold[6];  ///< Address of the old mesh node
  uint8_t       addr_mnew[6];  ///< Address of the new mesh node
  uint8_t       seq_no;   ///< sequence number, with addr_mnew unique
};

struct click_brn_iapp_he {
  uint8_t       addr_sta[6];      ///< Address of the station
  uint8_t       addr_ap_curr[6];  ///< Address of the current ap
  uint8_t       addr_ap_cand[6];  ///< Address of the ap candidate
  uint8_t       authoritive;      ///< Whether it is a request or response
};

struct click_brn_iapp {
  uint8_t       type;     ///< Type discriminator
  uint8_t       reserved[1]; ///< Reserved
  uint16_t      payload_len; ///< length of the payload, if any.

#define CLICK_BRN_IAPP_HON  1  ///< handover notify
#define CLICK_BRN_IAPP_HOR  2  ///< handover reply
#define CLICK_BRN_IAPP_DAT  3  ///< handover data
#define CLICK_BRN_IAPP_HRU  4  ///< handover route update
#define CLICK_BRN_IAPP_HEL  5  ///< iapp hello

  union {
    click_brn_iapp_ho ho; ///< payload for types 1-4
    click_brn_iapp_he he; ///< payload for type 5
  } payload;

  // used for transferring additional payload during hand over
  uint8_t       payload_type;  ///< used for additional payload
#define CLICK_BRN_IAPP_PAYLOAD_EMPTY    0  ///< no payload
#define CLICK_BRN_IAPP_PAYLOAD_GATEWAY  1  ///< payload from gateway
};

class BRNIAPProtocol : public Element { public:

  BRNIAPProtocol();
  ~BRNIAPProtocol();

  const char *class_name() const	{ return "BRNIAPProtocol"; }

};

CLICK_ENDDECLS
#endif
