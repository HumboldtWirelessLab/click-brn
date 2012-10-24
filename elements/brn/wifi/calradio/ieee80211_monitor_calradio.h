#ifndef _NET80211_IEEE80211_MONITOR_CALRADIO_H_
#define _NET80211_IEEE80211_MONITOR_CALRADIO_H_

struct calradio_header {
    int8_t  rssi;
    int8_t  rate;
} __attribute__ ((packed));

#endif
