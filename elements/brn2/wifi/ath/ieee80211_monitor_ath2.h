#ifndef _NET80211_IEEE80211_MONITOR_ATH2_H_
#define _NET80211_IEEE80211_MONITOR_ATH2_H_


#define MADWIFI_RXTX_FLAGS_SHORT_PREAMBLE   1 << 0


struct ath2_rx_status {
    u_int16_t	rs_datalen; /* rx frame length */
    u_int8_t	rs_status;  /* rx status, 0 => recv ok */
    u_int8_t	rs_phyerr;  /* phy error code */

    int8_t	rs_rssi;    /* rx frame RSSI (combined for 11n) */
    u_int8_t	rs_keyix;   /* key cache index */
    u_int8_t	rs_rate;    /* h/w receive rate index */
    u_int8_t	rs_more;    /* more descriptors follow */

    u_int32_t	rs_tstamp;  /* h/w assigned timestamp */
    u_int32_t	rs_antenna; /* antenna information */

    u_int64_t	rs_hosttime;
    u_int64_t	rs_mactime;

    int8_t	rs_noise;
    int8_t	rs_channel;
    int8_t	rs_flags;   /* preample len,... */
    int8_t	reserved[1];

} __attribute__ ((packed));

struct ath2_tx_status {
    u_int16_t	ts_seqnum;    /* h/w assigned sequence number */
    u_int16_t	ts_tstamp;    /* h/w assigned timestamp */

    u_int8_t	ts_status;    /* frame status, 0 => xmit ok */
    u_int8_t	ts_rate;      /* h/w transmit rate index */
    int8_t	ts_rssi;      /* tx ack RSSI */
    u_int8_t	ts_shortretry;/* # short retries */

    u_int8_t	ts_longretry; /* # long retries */
    u_int8_t	ts_virtcol;   /* virtual collision count */
    u_int8_t	ts_antenna;   /* antenna information */
    u_int8_t	ts_finaltsi;  /* final transmit series index */

    u_int64_t	ts_hosttime;
    u_int64_t	ts_mactime;

    int8_t	ts_noise;
    int8_t	ts_channel;
    int8_t	ts_flags;     /*short preamble,....*/
    int8_t	reserved[1];

} __attribute__ ((packed));
	     

struct ath2_tx_anno {

    int8_t operation;       //we use packets to configure the mac

    int8_t channel;         //channel to set

    u_int8_t mac[6];        //mac address use for sending or set as client for VA

    u_int8_t va_position;   //position in VA

} __attribute__ ((packed));

struct ath2_rx_anno {

  int8_t operation;       //we use packets to configure the mac

  int8_t channel;         //channel to set

  u_int8_t mac[6];        //mac address use for sending or set as client for VA

  u_int8_t va_position;   //position in VA

  u_int8_t status;

} __attribute__ ((packed));


/*************************************************/
/***************** DRIVER FLAGS ******************/
/*************************************************/


#define MADWIFI_FLAGS_CCA_ENABLED           1 << 0
#define MADWIFI_FLAGS_SMALLBACKOFF_ENABLED  1 << 1
#define MADWIFI_FLAGS_BURST_ENABLED         1 << 2
#define MADWIFI_FLAGS_CHANNELSWITCH_ENABLED 1 << 3
#define MADWIFI_FLAGS_MACCLONE_ENABLED      1 << 4

#define MADWIFI_FLAGS_IS_OPERATION          1 << 31

struct ath2_header {
    u_int16_t ath2_version;
    u_int16_t madwifi_version;

    u_int32_t flags;                        //driver flags

    union {
      struct ath2_rx_status rx;             //info of received packets
      struct ath2_tx_status tx;             //inof of txfeedbackpackets
      struct ath2_tx_anno tx_anno;          //annos of send packets
      struct ath2_rx_anno rx_anno;          //annos of operation packets (result)
    } anno;

} __attribute__ ((packed));

#define ATHDESC2_VERSION 0xF3F3

#define MADWIFI_0940	0x03ac
#define MADWIFI_3869	0x0f1d
#define MADWIFI_3880    0x0f28
#define MADWIFI_4133    0x1025

#define MADWIFI_TRUNK MADWIFI_4133

#define ATH2_OPERATION_NONE        0
#define ATH2_OPERATION_SETVACLIENT 1
#define ATH2_OPERATION_SETCHANNEL  2
#define ATH2_OPERATION_SETFLAGS    3

#ifndef ARPHRD_IEEE80211_ATHDESC2
#define ARPHRD_IEEE80211_ATHDESC2  805 /* IEEE 802.11 + atheros (long) descriptor */
#endif /* ARPHRD_IEEE80211_ATHDESC2 */

#define ATHDESC2_BRN_HEADER_SIZE sizeof(struct ath2_header)
#define ATHDESC2_HEADER_SIZE    ( ATHDESC_HEADER_SIZE + ATHDESC2_BRN_HEADER_SIZE )

#endif
