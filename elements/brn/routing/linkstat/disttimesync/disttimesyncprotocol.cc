#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

#include "disttimesyncprotocol.hh"
#include "disttimesync.hh"

CLICK_DECLS

int
DistTimeSyncProtocol::pack(struct pkt_bundle *bundle, char *buf, int size)
{
  if (size < (int) sizeof(struct pkt_bundle))
    return 0;
  else
    memcpy(buf, (uint8_t*) bundle, sizeof(struct pkt_bundle));

  return sizeof(struct pkt_bundle);
}

int
DistTimeSyncProtocol::unpack(struct pkt_bundle *bundle, char *buf, int size)
{
  if (size < (int) sizeof(struct pkt_bundle))
    return 0;
  else
    memcpy((uint8_t*) bundle, buf, sizeof(struct pkt_bundle));

  return sizeof(struct pkt_bundle);
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(DistTimeSyncProtocol)
