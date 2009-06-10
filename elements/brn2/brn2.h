#ifndef CLICK_BRN2_H
#define CLICK_BRN2_H

#include <clicknet/ether.h>
#include <click/etheraddress.hh>
/* MAJOR_Types */
#define BRN2_MAJOR_TYPE_STANDARD   0
#define BRN2_MAJOR_TYPE_LINKSTAT   1
#define BRN2_MAJOR_TYPE_ROUTING    2
#define BRN2_MAJOR_TYPE_DHT        3

#define BRN2_LINKSTAT_MINOR_TYPE_BEACON   1

#define BRN_PORT_BATMAN 10


struct brn2_packet_header{
  uint8_t major_src_type;
  uint8_t minor_src_type;
  uint8_t major_dst_type;
  uint8_t minor_dst_type;
  uint16_t size;
};

struct linkprobe {
  uint8_t sendernode[6];
  uint32_t id;
  uint16_t count_neighbors;
};

struct linkstateinfo {
  uint8_t neighbornode[6];
  uint32_t etx_metric;
};

#endif
