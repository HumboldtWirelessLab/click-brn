#ifndef DCLUSTERPROTOCOL_HH
#define DCLUSTERPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

#define DCLUSTER_INVALID_ROUND 255

struct dcluster_node_info {
  uint32_t id;
  uint8_t etheraddr[6];
  uint8_t hops;

  uint8_t round;
}__attribute__ ((packed));

struct dcluster_info {
  struct dcluster_node_info me;
  struct dcluster_node_info min;
  struct dcluster_node_info max;
}__attribute__ ((packed));


class DClusterProtocol : public Element { public:

  DClusterProtocol();
  ~DClusterProtocol();

  const char *class_name() const { return "DClusterProtocol"; }

  static int pack(struct dcluster_info *info, char *packet, int p_len);
  static int unpack(struct dcluster_info *info, char *packet, int p_len);

};

CLICK_ENDDECLS
#endif
