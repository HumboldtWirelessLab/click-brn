#ifndef CLICK_BRN2DSRPROTOCOL_HH
#define CLICK_BRN2DSRPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"

CLICK_DECLS

typedef Vector<EtherAddress> EtherAddresses;

/** Next is from common.hh */
class BRN2RouteQuerierHop
{
  public:
    EtherAddress ether_address;
    IPAddress ip_address;
    uint16_t _metric;

#define BRN_DSR_INVALID_HOP_METRIC 0xFF
#define BRN_DSR_INVALID_ROUTE_METRIC 9999
#define BRN_DSR_MIN_METRIC_RREQ_FWD 1000
#define BRN_DSR_MIN_LINK_METRIC_WITHIN_ROUTE 1000

    BRN2RouteQuerierHop(EtherAddress ether, uint16_t c) : ether_address(ether), ip_address(), _metric(c) {}
    BRN2RouteQuerierHop(EtherAddress ether, IPAddress ip, uint16_t c) : ether_address(ether), ip_address(ip), _metric(c) {}

    BRN2RouteQuerierHop(hwaddr ether) : ether_address(ether.data), ip_address(), _metric(BRN_DSR_INVALID_HOP_METRIC) {}
    BRN2RouteQuerierHop(hwaddr ether, struct in_addr ip) : ether_address(ether.data), ip_address(ip), _metric(BRN_DSR_INVALID_HOP_METRIC) {}

    BRN2RouteQuerierHop(hwaddr ether, uint16_t c) : ether_address(ether.data), ip_address(), _metric(c) {}
    BRN2RouteQuerierHop(hwaddr ether, struct in_addr ip, uint16_t c) : ether_address(ether.data), ip_address(ip), _metric(c) {}

    EtherAddress ether() const { return (const EtherAddress)ether_address; }
    IPAddress ip() const { return (const IPAddress)ip_address; }
};

typedef Vector<BRN2RouteQuerierHop> BRN2RouteQuerierRoute;

// info about route requests we're forwarding (if waiting for a
// unidirectionality test result) and those we've forwarded lately.
// kept in a hash table indexed by (src,target,id).  these entries
// expire after some time.
class ForwardedReqKey
{
  public:
    EtherAddress _src;
    EtherAddress _target;
    unsigned int _id;

    ForwardedReqKey(EtherAddress src, EtherAddress target, unsigned int id) {
      _src = src; _target = target; _id = id;
      check();
    }
    ForwardedReqKey() {
    // need this for bighashmap::pair to work
    }
    bool operator==(const ForwardedReqKey &f1) {
      check();
      f1.check();
      return ((_src    == f1._src) &&
          (_target == f1._target) &&
          (_id     == f1._id));
    }
    void check() const {
      assert(_src);
      assert(_target);
      assert(_src != _target);
    }
};

class ForwardedReqVal {
  public:
    timeval _time_forwarded;

    unsigned short best_metric; // best metric we've forwarded so far

  // the following two variables are set if we're waiting for a
  // unidirectionality test (RREQ with TTL 1) to come back
    timeval _time_unidtest_issued;
    Packet *p;

    void check() const {
//    assert(_time_forwarded.tv_usec > 0); //robert
//    assert(_time_forwarded.tv_sec > 0);  //robert
      assert(best_metric > 0);
    }
};

#define BRN_DSR_RREQ_TIMEOUT 600000 // how long before we timeout entries (ms)
#define BRN_DSR_RREQ_EXPIRE_TIMER_INTERVAL 15000 // how often to check (ms)

typedef HashMap<ForwardedReqKey, ForwardedReqVal> ForwardedReqMap;
typedef ForwardedReqMap::iterator FWReqIter;

/** End of common.hh */

struct click_dsr_hop {
  hwaddr  hw;
  uint16_t metric;
};

#ifdef CLICK_NS
// TODO adapt peek and all other things for max hop count != 16
# define BRN_DSR_MAX_HOP_COUNT  16
#else // CLICK_NS
// be reverse-compatible, we have already deployed nodes with this value
# define BRN_DSR_MAX_HOP_COUNT  16
#endif // CLICK_NS

/* DSR Route Request */
struct click_brn_dsr_rreq {
  uint16_t  dsr_id;
};

/* DSR Route Reply */
struct click_brn_dsr_rrep {
  uint8_t   dsr_flags;
  uint8_t   dsr_pad;
};

/* DSR Route Error */
struct click_brn_dsr_rerr {

#define BRN_DSR_RERR_TYPE_NODE_UNREACHABLE  1

  // broken link between dsr_unreachable_src and dsr_unreachable_dst
  hwaddr    dsr_unreachable_src;
  hwaddr    dsr_unreachable_dst;
  hwaddr    dsr_msg_originator; // TODO this info is implicit available in dsr_dst
  uint8_t   dsr_error;
  uint8_t   dsr_flags;
};

/* DSR Source Routed Packet */
struct click_brn_dsr_src {
  uint16_t      dsr_id;
  uint8_t       dsr_salvage;

  uint8_t       reserved; // @deprecated dsr_payload_type

#define BRN_DSR_PAYLOADTYPE_KEY 0

#define BRN_DSR_PAYLOADTYPE_RAW 0
#define BRN_DSR_PAYLOADTYPE_UDT 1
#define BRN_DSR_PAYLOADTYPE_DHT 2

#define BRN_DSR_PAYLOADTYPE_REST  10 /* for classifier's "-" argument. not a valid message type */

#define BRN_MAX_ETHER_LENGTH 1500
}; /* data */

/* Common DSR Structure */
struct click_brn_dsr {
  uint8_t       dsr_type;
  uint8_t       reserved;
  uint16_t      dsr_id;

  hwaddr        dsr_dst;
  hwaddr        dsr_src;

  /* in case of clients use their IPs */
  struct in_addr  dsr_ip_dst;
  struct in_addr  dsr_ip_src;

#define BRN_NOT_IP_NOT_AVAILABLE "0.0.0.0"
#define BRN_INTERNAL_NODE_IP "254.1.1.1"

#define BRN_DSR_RREQ 1
#define BRN_DSR_RREP 2
#define BRN_DSR_RERR 3
#define BRN_DSR_SRC  4

  union {
    click_brn_dsr_rreq rreq;
    click_brn_dsr_rrep rrep;
    click_brn_dsr_rerr rerr;
    click_brn_dsr_src src; /* data */
  } body;

  uint8_t       dsr_hop_count; /* total hop count */
  uint8_t       dsr_segsleft; /* hops left */
  click_dsr_hop addr[BRN_DSR_MAX_HOP_COUNT]; /* hops */
};

/* DSR Route Request */
//struct click_brn_dsr_rreq {
//  uint16_t reserved;
//};

/* DSR Route Reply */
//struct click_brn_dsr_rrep {
//  uint8_t   dsr_flags;
//  uint8_t   dsr_pad;
//};

/* DSR Route Error */
//struct click_brn_dsr_rerr {

  // broken link between dsr_unreachable_src and dsr_unreachable_dst
//  hwaddr    dsr_unreachable_src;
//  hwaddr    dsr_unreachable_dst;
//  hwaddr    dsr_msg_originator;
//  uint8_t   dsr_error;
//  uint8_t   dsr_flags;
//};

/* DSR Source Routed Packet */
//struct click_brn_dsr_src {
//#define BRN_DSR_ROUTE_TYPE_SRCROUTE         1
//#define BRN_DSR_ROUTE_TYPE_IDROUTE          2
//#define BRN_DSR_ROUTE_TYPE_SRCROUTE_SETID   3
//  uint8_t       dsr_route_type;

//empty packet to mark the route
//#define BRN_DSR_ROUTE_FLAG_EMPTY_PKT   1
//  uint8_t       flags;

//  uint8_t       dsr_salvage;            //looks like not really used
//  uint8_t       reserved;
//}; /* data */


#define BRN_DSR_PAYLOADTYPE_KEY 0

#define BRN_DSR_PAYLOADTYPE_RAW 0
#define BRN_DSR_PAYLOADTYPE_UDT 1
#define BRN_DSR_PAYLOADTYPE_DHT 2

#define BRN_DSR_PAYLOADTYPE_REST  10 /* for classifier's "-" argument. not a valid message type */

/*
 * Common structures, classes, etc.
 */
#define BRN_DSR_MEMORY_MEDIUM_METRIC        1     // static metric for in memory links
#define BRN_DSR_WIRED_MEDIUM_METRIC        10     // static metric for wired links
#define BRN_DSR_WIRELESS_MEDIUM_METRIC    100     // static metric for wireless links

#define BRN_DSR_STATION_METRIC            100  ///< metric for assoc'd stations
#define BRN_DSR_ROAMED_STATION_METRIC    5000  ///< metric for assoc'd stations

class DSRProtocol : public Element { public:

  DSRProtocol();
  ~DSRProtocol();

  const char *class_name() const	{ return "DSRProtocol"; }

  static int header_length(Packet *p);
  static click_dsr_hop* get_hops(const Packet *p);
  static click_dsr_hop* get_hops(click_brn_dsr *brn_dsr);

};

CLICK_ENDDECLS
#endif
