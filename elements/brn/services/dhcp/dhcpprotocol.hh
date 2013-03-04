#ifndef CLICK_BRNDHCPPROTOCOL_HH
#define CLICK_BRNDHCPPROTOCOL_HH
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

#include "dhcp.h"

CLICK_DECLS

#define DEFAULT_TTL 128
#define DEFAULT_TOS 0

#define BRN_HEADER_SIZE sizeof(click_brn)

class DHCPProtocol  {
 public:

  static WritablePacket *new_dhcp_packet(void);

  static int set_dhcp_header(Packet *p, uint8_t _op );

  static int add_option(Packet *packet, int option_num,int option_size,uint8_t *option_val);
  static void del_all_options(Packet *p);

  static unsigned char *getOptionsField(Packet *packet);
  static unsigned char *getOption(Packet *packet, int option,int *option_size);

  static void padOptionsField(Packet *packet);

  static int retrieve_dhcptype(Packet *packet);

  static void insertMagicCookie(Packet *p);
};

CLICK_ENDDECLS
#endif
