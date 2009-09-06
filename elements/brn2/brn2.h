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

#endif
