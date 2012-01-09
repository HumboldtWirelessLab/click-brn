#ifndef CLICK_LPRPROTOCOL_HH
#define CLICK_LPRPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

/** TODO: DOn't make everything statis. Use object do avoid creation and destruction of the object evry punk/unpack */

#define VERSION_BASE         0
#define VERSION_BASE_MAC_LZW 1

struct packed_link_header {
#if BYTE_ORDER == BIG_ENDIAN
  unsigned char _version:4;
  unsigned char _reserved:4;
#else
  unsigned char _reserved:4;
  unsigned char _version:4;
#endif

  unsigned char _no_nodes;
} __attribute__ ((packed));

struct etheraddr {
  unsigned char _addr[6];
} __attribute__ ((packed));


struct packed_link_info {
  struct packed_link_header *_header;

  struct etheraddr *_macs;
  unsigned char *_timestamp;
  unsigned char *_links;
};


class LPRProtocol : public Element { public:

  LPRProtocol();
  ~LPRProtocol();

  const char *class_name() const	{ return "LPRProtocol"; }

  static int pack(struct packed_link_info *info, unsigned char *packet, int p_len);
  static struct packed_link_info *unpack(unsigned char *packet, int p_len);

  static int pack2(struct packed_link_info *info, unsigned char *packet, int p_len);
  static struct packed_link_info *unpack2(unsigned char *packet, int p_len);

};

CLICK_ENDDECLS
#endif
