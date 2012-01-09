#ifndef CLICK_BRN2_H
#define CLICK_BRN2_H

#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/timestamp.hh>
#include <click/string.hh>
#include <click/straccum.hh>

CLICK_DECLS

#ifndef min
#define min(x,y)      ((x)<(y) ? (x) : (y))
#endif

#ifndef max
#define max(x,y)      ((x)>(y) ? (x) : (y))
#endif

#define BRN_VERSION "0.0.1"

#define BRN2_LINKSTAT_MINOR_TYPE_BEACON       1
#define BRN2_LINKSTAT_MINOR_TYPE_LPR          2
#define BRN2_LINKSTAT_MINOR_TYPE_RXC          3
#define BRN2_LINKSTAT_MINOR_TYPE_GEOR         4
#define BRN2_LINKSTAT_MINOR_TYPE_DCLUSTER     5
#define BRN2_LINKSTAT_MINOR_TYPE_NHPCLUSTER   6
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_DART    10
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_FALCON  11
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_KLIBS   12
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_NOFUDIS 13
#define BRN2_LINKSTAT_MINOR_TYPE_DHT_OMNI    14

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

class StringTokenizer {
  public:
    StringTokenizer(const String &s) {
      buf = new char[s.length()];
      memcpy(buf, s.data(), s.length());
      curr_ptr = buf;
      length = s.length();
    }

    bool hasMoreTokens() const {
      return curr_ptr < (buf + length);
    }

    ~StringTokenizer() {
      delete buf;
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
    EtherAddress *a = (EtherAddress *)va, *b = (EtherAddress *)vb;

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

const uint8_t brn_ethernet_broadcast[] = { 255,255,255,255,255,255 };
const EtherAddress brn_etheraddress_broadcast = EtherAddress(brn_ethernet_broadcast);

#define ETHERADDRESS_BROADCAST brn_etheraddress_broadcast

CLICK_ENDDECLS

#endif
