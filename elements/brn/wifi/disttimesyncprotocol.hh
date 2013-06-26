#ifndef DISTTIMESYNCPROTOCOL_HH
#define DISTTIMESYNCPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

#include "elements/brn/wifi/disttimesync.hh"

#define BUNDLE_SIZE 5

CLICK_DECLS

struct pkt_bundle {
  struct PacketSyncInfo pkts[BUNDLE_SIZE];
} __attribute((packed));


class DistTimeSyncProtocol {
public:
  static int pack  (struct pkt_bundle *bundle, char *buf, int size);
  static int unpack(struct pkt_bundle *bundle, char *buf, int size);
};

CLICK_ENDDECLS
#endif /* DISTTIMESYNCPROTOCOL_HH */
