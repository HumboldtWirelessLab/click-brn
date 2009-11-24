#ifndef CLICK_BRN2_H
#define CLICK_BRN2_H

#include <clicknet/ether.h>
#include <click/etheraddress.hh>

#ifndef min
#define min(x,y)      ((x)<(y) ? (x) : (y))
#endif

#ifndef max
#define max(x,y)      ((x)>(y) ? (x) : (y))
#endif


/** TODO: move next to brnprotocol */
/* MAJOR_Types */
#define BRN2_MAJOR_TYPE_STANDARD   0
#define BRN2_MAJOR_TYPE_LINKSTAT   1
#define BRN2_MAJOR_TYPE_ROUTING    2
#define BRN2_MAJOR_TYPE_DHT        3

#define BRN2_LINKSTAT_MINOR_TYPE_BEACON   1

#define BRN_PORT_BATMAN 10

/**TODO: Remove this or better move to other places */
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)P)
#endif

/*
 * Common structures, classes, etc.
 */
#define BRN_DSR_MEMORY_MEDIUM_METRIC        1     // static metric for in memory links
#define BRN_DSR_WIRED_MEDIUM_METRIC        10     // static metric for wired links
#define BRN_DSR_WIRELESS_MEDIUM_METRIC    100     // static metric for wireless links

#define BRN_DSR_STATION_METRIC            100  ///< metric for assoc'd stations
#define BRN_DSR_ROAMED_STATION_METRIC    5000  ///< metric for assoc'd stations

#endif
