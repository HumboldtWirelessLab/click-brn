#ifndef CLICK_BRN2_H
#define CLICK_BRN2_H

#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/timestamp.hh>
#include <click/string.hh>
#include <click/straccum.hh>

#include <elements/brn/version.h>

#ifndef BRN_GIT_VERSION
#define BRN_GIT_VERSION "n/a"
#endif

CLICK_DECLS

#ifndef MIN
#define MIN(x,y)      ((x)<(y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y)      ((x)>(y) ? (x) : (y))
#endif

#define BRN_VERSION "0.0.1"

#define BRN2_LINKSTAT_MINOR_TYPE_BEACON       1
#define BRN2_LINKSTAT_MINOR_TYPE_LPR          2
#define BRN2_LINKSTAT_MINOR_TYPE_RXC          3
#define BRN2_LINKSTAT_MINOR_TYPE_GPS          4
#define BRN2_LINKSTAT_MINOR_TYPE_DCLUSTER     5
#define BRN2_LINKSTAT_MINOR_TYPE_NHPCLUSTER   6
#define BRN2_LINKSTAT_MINOR_TYPE_GEOR         7
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_DART    10
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_FALCON  11
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_KLIBS   12
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_NOFUDIS 13
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_OMNI    14
#define BRN2_LINKSTAT_MINOR_TYPE_TIMESYNC    15
#define BRN2_LINKSTAT_MINOR_TYPE_BROADCAST   16

/**TODO: Remove this or better move to other places */
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)P)
#endif

/*
 * Common structures, classes, etc.
 */
#define BRN_LT_MEMORY_MEDIUM_METRIC        1     // static metric for in memory links
#define BRN_LT_WIRED_MEDIUM_METRIC        10     // static metric for wired links
#define BRN_LT_WIRELESS_MEDIUM_METRIC    100     // static metric for wireless links

#define BRN_LT_STATION_METRIC            100  ///< metric for assoc'd stations
#define BRN_LT_ROAMED_STATION_METRIC    5000  ///< metric for roamed stations
#define BRN_LT_INVALID_LINK_METRIC      9999  ///< metric for invalid/broken links
#define BRN_LT_INVALID_ROUTE_METRIC    65535  ///< metric for invalid/broken routes

#define BRN_LT_DEFAULT_MIN_METRIC_IN_ROUTE    4000  ///< metric for invalid/broken routes

#define BRN_ROUTING_MAX_HOP_COUNT        100

#define BRN_MAX_ETHER_LENGTH 1500

class StringTokenizer {
  public:
    explicit StringTokenizer(const String &s) {
      buf = new char[s.length()];
      memcpy(buf, s.data(), s.length());
      curr_ptr = buf;
      length = s.length();
    }

    StringTokenizer(const StringTokenizer &st) {
      buf = new char[st.length];
      memcpy(buf, st.buf, st.length);
      curr_ptr = buf;
      length = st.length;
    }

    bool hasMoreTokens() const {
      return curr_ptr < (buf + length);
    }

    ~StringTokenizer() {
      delete[] buf;
    }

    String getNextToken() {
      char tmp_buf[256];
      memset(tmp_buf, '\0', 256);
      int currIndex = 0;

      while ( curr_ptr < (buf + length) ) {
        if (*curr_ptr == ' ' || *curr_ptr == '\n') {
          curr_ptr++;
          break;
        }
        tmp_buf[currIndex++] = *curr_ptr;
        curr_ptr++;
      }
      tmp_buf[currIndex] = '\0';

      String s(tmp_buf);
      return s;
    }

  private:
    char *buf;
    char *curr_ptr;
    int length;
};

extern "C" {
  static inline int etheraddr_sorter(const void *va, const void *vb, void */*thunk*/) {
    const EtherAddress *a = reinterpret_cast<const EtherAddress *>(va);
    const EtherAddress *b = reinterpret_cast<const EtherAddress *>(vb);

    if (a == b) {
      return 0;
    }

    uint8_t * addr_a = (uint8_t *)a->data();
    uint8_t * addr_b = (uint8_t *)b->data();

    for (int i = 0; i < 6; i++) {
      if (addr_a[i] < addr_b[i])
        return -1;
      else if (addr_a[i] > addr_b[i])
        return 1;
    }
    return 0;
  }
}

extern "C" {
  static inline int32_t isqrt32(int32_t n) {
    int32_t x,x1;

    if ( n == 0 ) return 0;
    if ( n < 0 ) return -1;

    x1 = n;
    do {
      x = x1;
      x1 = (x + n/x) >> 1;
    } while ((( (x - x1) > 1 ) || ( (x - x1)  < -1 )) && ( x1 != 0 ));

    return x1;
  }
}

extern "C" {
  static inline int64_t isqrt64(int64_t n) {
    int64_t x,x1;

    if ( n == 0 ) return 0;
    if ( n < 0 ) return -1;

    x1 = n;
    do {
      x = x1;
      x1 = (x + n/x) >> 1;
    } while ((( (x - x1) > 1 ) || ( (x - x1)  < -1 )) && ( x1 != 0 ));

    return x1;
  }
}

extern "C" {
  static inline void str_replace(String &s, char c, char replace) {
    char *s_data = s.mutable_data();

    for(int i = 0; i < s.length(); i++) {
      if ( s_data[i] == c ) s_data[i] = replace;
    }
  }
}

const uint8_t brn_ethernet_broadcast[] = { 255,255,255,255,255,255 };
const EtherAddress brn_etheraddress_broadcast = EtherAddress(brn_ethernet_broadcast);

#define ETHERADDRESS_BROADCAST brn_etheraddress_broadcast

#define BRN_NOT_IP_NOT_AVAILABLE "0.0.0.0"
#define BRN_INTERNAL_NODE_IP "254.1.1.1"

/**
 * BRN Typedefs
 */

typedef HashMap<EtherAddress, Timestamp> EtherTimestampMap;
typedef EtherTimestampMap::const_iterator EtherTimestampMapIter;


CLICK_ENDDECLS

#endif
