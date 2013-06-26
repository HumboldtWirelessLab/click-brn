#ifndef DISTTIMESYNC_HH
#define DISTTIMESYNC_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <click/packet.hh>
#include <click/timer.hh>
#include <clicknet/wifi.h>

#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

#define DISTTIMESYNC_INIT 0
#define TD_RANDOM -1
#define ETHER_ADDR_SIZE 6  /* length of a MAC Address in bytes */

#define PKT_BUF_SIZE 16    /* packet buffer size per node */
#define HST_BUF_SIZE 32    /* host time buffer size per node */

#define BUNDLE_SIZE 5      /* no. of pkts to be put in linkprobe pkts */

CLICK_DECLS

struct PacketSyncInfo {
  u_int8_t  src[ETHER_ADDR_SIZE]; /* sender EtherAddress */
  u_int16_t seq_no;               /* packet sequence no. */
  u_int64_t host_time;            /* packet k_time from the driver */
} __attribute((packed));
/* = 16 Byte */

struct HostTimeDiff {
  int64_t hst_diff;     /* the actual diff' of host times */
  Timestamp ts;         /* when did I receive the package */
  u_int32_t pkt_handle; /* packet handle to identify the package */
};

class HostTimeBuf {
public:
  struct HostTimeDiff *hst_diffs;
  u_int32_t hst_idx;

  HostTimeBuf();
  ~HostTimeBuf();
  int has_pkt(u_int32_t packet_handle);
};

typedef HashMap<EtherAddress, HostTimeBuf *> TimeDiffTable;
typedef HashMap<u_int32_t, u_int32_t> PacketIndexTable;

class DistTimeSync : public BRNElement {
public:
  DistTimeSync();
  ~DistTimeSync();

  const char *class_name() const { return "DistTimeSync"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &conf, ErrorHandler* errh);
  int initialize(ErrorHandler *);
  Packet *simple_action(Packet *p);

  int lpSendHandler(char *buf, int size);
  int lpReceiveHandler(char *buf, int size);

  void add_handlers();
  String stats_handler();

  u_int32_t create_packet_handle(u_int16_t seq_no, u_int8_t *src_addr);

  u_int32_t _time_drift;
  u_int32_t _pkt_buf_size;
  u_int32_t _pkt_buf_idx;

  TimeDiffTable tdt;
  PacketIndexTable pit;
  struct PacketSyncInfo *pkt_buf;

  BRN2LinkStat *_linkstat;
};

EtherAddress get_sender_addr(struct click_wifi *wh);
bool hst_buf_has_pkt(HostTimeBuf *hst_buf, u_int32_t packet_handle);

void print_info(Packet *p);

CLICK_ENDDECLS

#endif /* DISTTIMESYNC_HH */
