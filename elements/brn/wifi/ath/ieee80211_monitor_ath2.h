#ifndef _NET80211_IEEE80211_MONITOR_ATH2_H_
#define _NET80211_IEEE80211_MONITOR_ATH2_H_

struct ath2_rx_status {
    u_int16_t	rs_datalen; /* rx frame length */
    u_int8_t	rs_status;  /* rx status, 0 => recv ok */
    u_int8_t	rs_phyerr;  /* phy error code */
    int8_t	  rs_rssi;    /* rx frame RSSI (combined for 11n) */
    u_int8_t	rs_rate;    /* h/w receive rate index */
    u_int32_t	rs_tstamp;  /* h/w assigned timestamp */
    u_int32_t	rs_antenna; /* antenna information */

    int8_t	  noise;
    u_int64_t	hosttime;
    u_int64_t	mactime;
};

struct ath2_tx_status {
    u_int16_t	ts_seqnum;    /* h/w assigned sequence number */
    u_int16_t	ts_tstamp;    /* h/w assigned timestamp */
    u_int8_t	ts_status;    /* frame status, 0 => xmit ok */
    u_int8_t	ts_rate;      /* h/w transmit rate index */
    int8_t	  ts_rssi;      /* tx ack RSSI */
    u_int8_t	ts_shortretry;/* # short retries */
    u_int8_t	ts_longretry; /* # long retries */
    u_int8_t	ts_virtcol;   /* virtual collision count */
    u_int8_t	ts_antenna;   /* antenna information */
    u_int8_t	ts_finaltsi;  /* final transmit series index */

    int8_t    noise;
    u_int64_t hosttime;
    u_int64_t mactime;
};

struct ath2_tx_anno {
    int8_t channel;
    u_int8_t mac[6];
};


struct ath2_header {
    u_int16_t id;
    u_int16_t pad;

    union {
      struct ath2_rx_status rx;
      struct ath2_tx_status tx;
      struct ath2_tx_anno tx_anno;
    } anno; 

};

#define ATHDESC2_BRN_ID 0xF2F2

#endif
