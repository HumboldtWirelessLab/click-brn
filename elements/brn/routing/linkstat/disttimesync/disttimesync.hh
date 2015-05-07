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
#define ETHER_ADDR_SIZE 6    /* length of a MAC Address in bytes */

#define PKT_BUF_SIZE 16      /* packet buffer size per node */
#define HST_BUF_SIZE 64      /* host time buffer size per node */

#define BUNDLE_SIZE 5        /* no. of pkts to be put in linkprobes */

#define DTS_MAX_OFFSET 100   /* max. artificial offset when simulating */
#define DTS_MAX_TIMEDRIFT 20 /* max. artificial timedrift when simulating */

#define DTS_TD_CACHE_DURATION 1000000 /* usec  = 1000 ms = 1 s */

CLICK_DECLS

struct PacketSyncInfo {
  u_int8_t  src[ETHER_ADDR_SIZE]; /* sender EtherAddress */
  u_int16_t seq_no;               /* packet sequence no. */
  uint64_t host_time;             /* packet k_time from the driver */
}; /* = 16 Byte */

struct HostTimeTuple {
  uint64_t hst_me;   /* when did the probe rx'ing node receive the orig. pkt */
  uint64_t hst_nb;   /* when did the probing node receive the package */
  uint32_t pkt_handle; /* unique packet handle to identify the package */
}; /* = 20 Byte */

struct DriftInfo {
  double drift;
  int64_t offset;
  uint64_t ts;
};

class HostTimeBuf {
public:
  struct HostTimeTuple *hst_tpls;
  uint32_t hst_idx;
  uint32_t entries;

  struct DriftInfo di; /* most recent timedrift and offset + timestamp */

  HostTimeBuf();
  ~HostTimeBuf();

  int has_pkt(uint32_t packet_handle);

  double get_timedrift();
  int64_t get_offset();
};

typedef HashMap<EtherAddress, HostTimeBuf *> TupleBufTable;
typedef TupleBufTable::const_iterator TBTIter;

typedef HashMap<uint32_t, uint32_t> PacketIndexTable;
typedef PacketIndexTable::const_iterator PITIter;

class DistTimeSync : public BRNElement {
public:

  /* artificial time drift and offset used when simulating */
  int32_t _time_drift; /* 1.23 represented as 123 */
  int32_t _offset;

  /* size and index of the packet ring buffer used to store rx'd pkts */
  uint32_t _pkt_buf_size;
  uint32_t _pkt_buf_idx;

  /* mapping from MAC-Addr to host time tuple buffer */
  TupleBufTable tbt;

  /* mapping from packet handle to pkt (ring) buffer index */
  PacketIndexTable pit;

  struct PacketSyncInfo pkt_buf[PKT_BUF_SIZE];
  BRN2LinkStat *_linkstat;

  uint32_t node_id;

  DistTimeSync();
  ~DistTimeSync();

  const char *class_name() const { return "DistTimeSync"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &conf, ErrorHandler* errh);
  int initialize(ErrorHandler *);
  Packet *simple_action(Packet *p);

  /* link probe handler */
  int lpSendHandler(char *buf, int size);
  int lpReceiveHandler(char *buf, int size, EtherAddress src);

  /* stats handler for access to the calculated time drifts */
  void add_handlers();
  String stats_handler();

  /*
   * creates a unique packet handle consisting of the sequence number and
   * the last 2 bytes of the sender's MAC-Address:
   * [seq_no (2) | last 2 MAC (2)]
   */
  uint32_t create_pkt_handle(u_int16_t seq_no, u_int8_t *src_addr);
};

CLICK_ENDDECLS

#endif /* DISTTIMESYNC_HH */
