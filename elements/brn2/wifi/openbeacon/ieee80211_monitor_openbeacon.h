#ifndef _NET80211_IEEE80211_MONITOR_OPENBEACON_H_
#define _NET80211_IEEE80211_MONITOR_OPENBEACON_H_

struct Click2OBD_header {
    uint8_t  rssi;					// unused
    uint8_t  channel;                          // channel frequency:      2400 MHz + rf_ch * a MHz       ( a=1 für 1 Mbps, 2 für 2 Mbps )
    uint8_t  rate;				// data rate value:      	  1 = 1 Mbps   ,		2 = 2 Mbps
    uint8_t  power;        			// power:		        	00 =  -18 dBm,		01 = -12 dBm		10 = -6 dBm		11 = 0 dBm
    uint8_t  openbeacon_dmac[5];	// kann von 2-5 Byte variieren
    uint8_t  openbeacon_smac[5];	// kann von 2-5 Byte variieren
} __attribute__ ((packed));

#endif
