#ifndef GEORPROTOCOL_HH
#define GEORPROTOCOL_HH

#include "elements/brn/brnelement.hh"

#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

#include "elements/brn/services/sensor/gps/gps.hh"

CLICK_DECLS

#define GEOR_TYPE_ROUTING      0
#define GEOR_TYPE_ROUTEREQUEST 1
#define GEOR_TYPE_ROUTEREPLY   2

#define GEOR_DAFAULT_MAX_HOP_COUNT 45

struct geor_header {
  uint8_t type;
  uint8_t reserved;

  uint8_t src[6];
  struct gps_position src_pos;

  uint8_t dst[6];
  struct gps_position dst_pos;
} __attribute__ ((packed));


class GeorProtocol : public BRNElement { public:

  GeorProtocol();
  ~GeorProtocol();

  const char *class_name() const { return "GeorProtocol"; }

  static WritablePacket *addRoutingHeader(Packet *p,
                                          EtherAddress *sea, GPSPosition *spos,
                                          EtherAddress *dea, GPSPosition *dpos);

  static struct geor_header *getRoutingHeader(Packet *p) { return ((struct geor_header *)p->data()); }
  static void stripRoutingHeader(Packet *p) { p->pull(sizeof(struct geor_header)); }

};

CLICK_ENDDECLS
#endif
