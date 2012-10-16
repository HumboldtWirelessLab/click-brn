#ifndef NHOPCLUSTERPROTOCOL_HH
#define NHOPCLUSTERPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

/** Main header */

/**
  Management: detection, notification,....
  Routing:
*/

#define NHOPCLUSTER_PACKETTYPE_MANAGMENT 1
#define NHOPCLUSTER_PACKETTYPE_ROUTING   2
#define NHOPCLUSTER_PACKETTYPE_LP_INFO   3

struct nhopcluster_packet_header {
  uint8_t packet_type;
}__attribute__ ((packed));

/** Managment header: clusterhead selection */

#define NHOPCLUSTER_MANAGMENT_REQUEST      1
#define NHOPCLUSTER_MANAGMENT_REPLY        2
#define NHOPCLUSTER_MANAGMENT_NOTIFICATION 3
#define NHOPCLUSTER_MANAGMENT_REJECT       4
#define NHOPCLUSTER_MANAGMENT_INFO         5

struct nhopcluster_managment {
  uint8_t operation;
  uint8_t hops;
  uint8_t code;

  uint32_t id;
  uint8_t clusterhead[6];

}__attribute__ ((packed));

#define NHOPCLUSTER_REJECT_REASON_ALREADY_SELECTED 1

struct nhopcluster_lp_info {
  uint8_t hops;

  uint32_t id;
  uint8_t clusterhead[6];

}__attribute__ ((packed));


class NHopClusterProtocol : public Element { public:

  NHopClusterProtocol();
  ~NHopClusterProtocol();

  const char *class_name() const { return "NHopClusterProtocol"; }

  static int pack_lp(struct nhopcluster_lp_info *lpi, uint8_t *data, int max_size);
  static int unpack_lp(struct nhopcluster_lp_info *lpi, uint8_t *data, int size);

  static struct nhopcluster_managment *get_mgt(Packet *p, int offset);
  static int get_type(Packet *p, int offset);
  static int get_operation(Packet *p, int offset);

  static WritablePacket* new_request(int max_hops, int code, int id);
  static WritablePacket* new_notify(EtherAddress *ch, int max_hops, int code, int id);


};

CLICK_ENDDECLS
#endif
