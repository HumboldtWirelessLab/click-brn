#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "dclusterprotocol.hh"

CLICK_DECLS

DClusterProtocol::DClusterProtocol()
{
}

DClusterProtocol::~DClusterProtocol()
{
}

int
DClusterProtocol::pack(struct dcluster_info *info, char *packet, int p_len)
{
  if ( p_len < (int)sizeof(struct dcluster_info)) return 0;
  else memcpy(packet, (uint8_t*)info, sizeof(struct dcluster_info));

  return sizeof(struct dcluster_info);
}

int
DClusterProtocol::unpack(struct dcluster_info *info, char *packet, int p_len) {
  if ( p_len < (int)sizeof(struct dcluster_info)) return 0;
  else memcpy((uint8_t*)info, packet, sizeof(struct dcluster_info));

  return sizeof(struct dcluster_info);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DClusterProtocol)

