#ifndef DCLUSTERPROTOCOL_HH
#define DCLUSTERPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

/** Main header */

#define DCLUSTER_PACKETTYPE_INFO      1
#define DCLUSTER_PACKETTYPE_MANAGMENT 2
#define DCLUSTER_PACKETTYPE_ROUTING   3

struct dcluster_packet_header {
  uint8_t packet_type;
}__attribute__ ((packed));


/** The node info can send directly (broadcast) or per linkprobe */

#define DCLUSTER_INVALID_ROUND 255

struct dcluster_node_info {
  uint32_t id;
  uint8_t etheraddr[6];
  uint8_t hops;

  uint8_t round;
}__attribute__ ((packed));

struct dcluster_info {
  uint8_t src_etheraddr[6];
  struct dcluster_node_info me;
  struct dcluster_node_info min;
  struct dcluster_node_info max;
}__attribute__ ((packed));

/** Managment header: clusterhead selection */

#define DCLUSTER_MANAGMENT_SELECT_CH   1
#define DCLUSTER_MANAGMENT_REJECT_CH   2
#define DCLUSTER_MANAGMENT_LEAVE_CH    3
#define DCLUSTER_MANAGMENT_REDIRECT_CH 4
#define DCLUSTER_MANAGMENT_ACK_CH      5

/** If a node select a cluster head and another node finds a better one for this node, then it sends a
 redirect. The node then send a mgt_select to the new one. This node (ch) send an ack. */

struct dcluster_managment {
  uint8_t operation;
  uint8_t slave[6];
  uint8_t master[6];
}__attribute__ ((packed));

/** Routing header: */
struct dcluster_routing_header {
  uint8_t hops;
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t clh[6];
}__attribute__ ((packed));


class DClusterProtocol {
 public:
  static int pack(struct dcluster_info *info, char *packet, int p_len);
  static int unpack(struct dcluster_info *info, char *packet, int p_len);

};

CLICK_ENDDECLS
#endif
