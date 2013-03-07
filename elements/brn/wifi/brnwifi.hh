#ifndef _BRNWIFI_HH_
#define _BRNWIFI_HH_

#include <click/config.h>
#include <clicknet/wifi.h>

#include "elements/brn/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

enum {
  //TODO: compress error types (there will never be received a combination of them
  WIFI_EXTRA_RX_CRC_ERR      = (1<<31), /* failed crc check */
  WIFI_EXTRA_RX_PHY_ERR      = (1<<30), /* failed phy check */
  WIFI_EXTRA_RX_FIFO_ERR     = (1<<29), /* failed fifo check */
  WIFI_EXTRA_RX_DECRYPT_ERR  = (1<<28), /* failed decrypt check */
  WIFI_EXTRA_RX_MIC_ERR      = (1<<27), /* failed mic check */
  WIFI_EXTRA_RX_ZERORATE_ERR = (1<<26), /* has zero rate */
  WIFI_EXTRA_SHORT_PREAMBLE  = (1<<25),
  WIFI_EXTRA_MCS_RATE3       = (1<<24),
  WIFI_EXTRA_MCS_RATE2       = (1<<23),
  WIFI_EXTRA_MCS_RATE1       = (1<<22),
  WIFI_EXTRA_MCS_RATE0       = (1<<21),
  WIFI_EXTRA_EXT_RX_STATUS   = (1<<20),
  WIFI_EXTRA_TX_QUEUE_HIGH   = (1<<19),
  WIFI_EXTRA_TX_QUEUE_LOW    = (1<<18),
  WIFI_EXTRA_JAMMER_MESSAGE  = (1<<17),

  WIFI_EXTRA_RX_PHANTOM_ERR  = (WIFI_EXTRA_RX_ZERORATE_ERR | WIFI_EXTRA_RX_PHY_ERR)
};

#define WIFI_EXTRA_FLAG_MCS_RATE_START 21
#define WIFI_EXTRA_TX_QUEUE_START 18

/****************************/
/* BRN wifi extra extention */
/****************************/

/* For eac rate one entry
 *
 * Description:
 *
 * Bit         function
 *  0           FEC ( 0 - BCC   1 - LDPC )
 *  1           HT-mode  ( 0 - mixed   1 - greenfield )
 *  2           Short Premable ( 0 - long   1 - short )
 *  3           STBC ( 0 - "normal"  1 - use space-time-block-code )
 *  4
 *  5
 *  6
 *  7
 *
*/

struct brn_click_wifi_extra_extention {
  uint8_t mcs_flags[4];
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct brn_click_wifi_extra_rx_status {
  int8_t rssi_ctl[3];
  int8_t rssi_ext[3];
  int8_t evm[5];
  int8_t flags;
} __attribute__((__packed__));

#define IEEE80211_FEC_INDEX 0
#define IEEE80211_FEC       (1<<IEEE80211_FEC_INDEX)
#define IEEE80211_FEC_BCC   0
#define IEEE80211_FEC_LDPC  1

#define IEEE80211_HT_MODE_INDEX      1
#define IEEE80211_HT_MODE            (1<<IEEE80211_HT_MODE_INDEX)
#define IEEE80211_HT_MODE_MIXED      0
#define IEEE80211_HT_MODE_GREENFIELD 1

#define IEEE80211_PREAMBLE_LENGTH_INDEX 2
#define IEEE80211_PREAMBLE_LENGTH       (1<<IEEE80211_PREAMBLE_LENGTH_INDEX)
#define IEEE80211_PREAMBLE_LENGTH_LONG  0
#define IEEE80211_PREAMBLE_LENGTH_SHORT 1

#define IEEE80211_STBC_INDEX   3
#define IEEE80211_STBC         (1<<IEEE80211_STBC_INDEX)
#define IEEE80211_STBC_DISABLE 0
#define IEEE80211_STBC_ENABLE  1


/* Bandwidth stuff */

#define IEEE80211_MCS_BW_20               0
#define IEEE80211_MCS_BW_40               1
#define IEEE80211_MCS_BW_20L              2
#define IEEE80211_MCS_BW_20U              3

#define IEEE80211_GUARD_INTERVAL_LONG     0
#define IEEE80211_GUARD_INTERVAL_SHORT    1

/*
-------------------------------------------------------------------
|   0   |   1   |   2   |   3   |   4   |   5   |   6  |    7     |
|                MCS INDEX              |   BANDWIDTH  | GUARDINT |
-------------------------------------------------------------------
*/


struct wifi_n_ht_control {
  uint16_t link_adaption;
  uint8_t  calibration;
  uint8_t  aggregation;
} CLICK_SIZE_PACKED_ATTRIBUTE;



/* QOS-Field 1
  | RESERVER (Bit7) | ACK_Policy (Bit 5-6) | EOSP (Bit 4) | TID (Bit 0-3) |

TID
  0 Besteffort
  1-2 Background
  3-5 Video
  6-7 Voice

  QOS-Field 2
  Queue-Size
  Transmit Opprtunity

*/

#define QOS_PRIORITY_BEST_EFFORT 0

#define NORMAL_ACK      0 << 5
#define NO_ACK          1 << 5
#define NO_EXPLICIT_ACK 2 << 5
#define BLOCK_ACK       3 << 5

struct wifi_qos_field {
  uint8_t qos;
  uint8_t queue;
} CLICK_SIZE_PACKED_ATTRIBUTE;

struct wifi_n_msdu_header {
  uint8_t  da[6];
  uint8_t  sa[6];
  uint16_t len;
} CLICK_SIZE_PACKED_ATTRIBUTE;


class BrnWifi
{

 public:

  BrnWifi();
  ~BrnWifi();


  static inline bool hasExtRxStatus(struct click_wifi_extra *ceh ) {
    return ((ceh->flags & WIFI_EXTRA_EXT_RX_STATUS) != 0 );
  }

  static inline void setExtRxStatus(struct click_wifi_extra *ceh, uint8_t ext_rx_status) {
    if (ext_rx_status)
      ceh->flags |= (uint32_t)WIFI_EXTRA_EXT_RX_STATUS;
    else
      ceh->flags &= ~(uint32_t)WIFI_EXTRA_EXT_RX_STATUS;
  }

  /* Single bit signals, whether rate is mcs-rate or "normal" rate */
  static inline uint8_t getMCS(struct click_wifi_extra *ceh, int index) {
    return (ceh->flags >> (WIFI_EXTRA_FLAG_MCS_RATE_START+index) & 1);
  }

  static inline void setMCS(struct click_wifi_extra *ceh, int index, uint8_t mcs) {
    if (mcs) {
      ceh->flags |= (uint32_t)1 << (WIFI_EXTRA_FLAG_MCS_RATE_START+index);
    } else {
      ceh->flags &= ~((uint32_t)1 << (WIFI_EXTRA_FLAG_MCS_RATE_START+index));
    }
  }

  /* Convert rate to/from mcs-rate (rate-index, bw, guard_interval) */
  static inline void fromMCS(uint8_t mcs_index, uint8_t bandwidth, uint8_t guard_interval, uint8_t *rate) {
    *rate = (mcs_index & 31) | ((bandwidth & 3) << 5) | ((guard_interval & 1) << 7);
  }

  static inline void toMCS(uint8_t *mcs_index, uint8_t *bandwidth, uint8_t *guard_interval, uint8_t rate) {
    *mcs_index = rate & 31;
    *bandwidth = (rate & 96) >> 5;
    *guard_interval = (rate & 128) >> 7;
  }

  /***************************************************/
  /* MCS flags, which have no impact on the datarate */
  /***************************************************/
  static inline void clear_click_wifi_extra_extention(struct brn_click_wifi_extra_extention *ceh) {
    memset((void*)ceh, 0, sizeof(struct brn_click_wifi_extra_extention));
  }

  static inline struct brn_click_wifi_extra_extention *get_brn_click_wifi_extra_extention(Packet *p) {
    return (struct brn_click_wifi_extra_extention *)BRNPacketAnno::get_brn_wifi_extra_extention_anno(p);
  }

  static inline uint8_t getFEC(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index]) & 1);
  }

  static inline void setFEC(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t fec_type) {
    if ( fec_type )
      ceh->mcs_flags[index] |= (uint8_t)1;    // set flag
    else
      ceh->mcs_flags[index] &= ~((uint8_t)1); // clear flag
  }

  static inline uint8_t getHTMode(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] >> IEEE80211_HT_MODE_INDEX) & 1);
  }

  static inline void setHTMode(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t htmode) {
    if (htmode)
      ceh->mcs_flags[index] |= ((uint8_t)1) << IEEE80211_HT_MODE_INDEX;
    else
      ceh->mcs_flags[index] &= ~(((uint8_t)1) << IEEE80211_HT_MODE_INDEX);
  }

  static inline uint8_t getPreambleLength(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] >> IEEE80211_PREAMBLE_LENGTH_INDEX) & 1);
  }

  static inline void setPreambleLength(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t pl) {
    if (pl)
      ceh->mcs_flags[index] |= ((uint8_t)1) << IEEE80211_PREAMBLE_LENGTH_INDEX;
    else
      ceh->mcs_flags[index] &= ~(((uint8_t)1) << IEEE80211_PREAMBLE_LENGTH_INDEX);
  }

  static inline uint8_t getSTBC(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] >> IEEE80211_STBC_INDEX) & 1);
  }

  static inline void setSTBC(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t stbc) {
    if (stbc)
      ceh->mcs_flags[index] |= ((uint8_t)1) << IEEE80211_STBC_INDEX;
    else
      ceh->mcs_flags[index] &= ~(((uint8_t)1) << IEEE80211_STBC_INDEX);
  }

  /* Functions for rate calculation */
  static inline int getMCSRate(uint8_t mcs_index, uint8_t bandwidth, uint8_t guard_interval) {
	  /*
	  * This is the table of data-rates with MCS-Mode,
	  * representing the first block with one spatial stream.
	  * Because of linearity the blocks for spatial
	  * stream 2,3 and 4 are calculated through multipl.
	  */

	  //		20MHz	20MHz	40MHz	40MHz
	  //		800ns	400ns	800ns	400ns
	  //		-----------------------------
	  int rates[] = {	65, 	72, 	135, 	150,
			  130,	144, 	270, 	300,
			  195, 	217, 	405, 	450,
			  260, 	289, 	540, 	600,
			  390, 	433, 	810, 	900,
			  520, 	578, 	1080, 	1200,
			  585, 	650, 	1215, 	1350,
			  650, 	722, 	1350, 	1500 };

	  //click_chatter("%d %d %d",idx,bw,gi);

    return ( (mcs_index >> 3) + 1) * rates[ ((mcs_index & 7) << 2) + (bandwidth << 1) + guard_interval ];

  }

  static inline int getRate(struct click_wifi_extra *ceh, int index) {
    if ( ceh->flags & ( 1 << (WIFI_EXTRA_FLAG_MCS_RATE_START+index)) ) {
      uint8_t mcs_index, bandwidth, guard_interval;

      switch (index) {
        case 0: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate); break;
        case 1: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate1); break;
        case 2: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate2); break;
        case 3: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate3); break;
        default: click_chatter("Unknown rate index"); return 0;
      }

      return getMCSRate(mcs_index, bandwidth, guard_interval);

    } else {
      switch (index) {
        case 0: return (ceh->rate * 5);
        case 1: return (ceh->rate1 * 5);
        case 2: return (ceh->rate2 * 5);
        case 3: return (ceh->rate3 * 5);
      }
    }

    click_chatter("Unknown rate index");
    return 0;
  }

  static uint32_t pkt_duration(uint32_t pktlen, uint8_t rix, uint8_t width, uint8_t half_gi);

  static inline void setTxQueue(struct click_wifi_extra *ceh, uint32_t queue) {
    ceh->flags &= ~(3 << WIFI_EXTRA_TX_QUEUE_START );   //clear old txqueue info
    ceh->flags |= (queue << WIFI_EXTRA_TX_QUEUE_START); //set new txqueue
  }

  static inline uint8_t getTxQueue(struct click_wifi_extra *ceh) {
    return (ceh->flags >> WIFI_EXTRA_TX_QUEUE_START) & 3; //get txqueue
  }

  static inline bool isJammer(struct click_wifi_extra *ceh ) {
    return ((ceh->flags & WIFI_EXTRA_JAMMER_MESSAGE) != 0 );
  }

  static inline void setJammer(struct click_wifi_extra *ceh, bool jammer) {
    if (jammer)
      ceh->flags |= (uint32_t)WIFI_EXTRA_JAMMER_MESSAGE;
    else
      ceh->flags &= ~(uint32_t)WIFI_EXTRA_JAMMER_MESSAGE;
  }


  /* returns whether rate (0), rate1, .., rate3 has been used */
  static inline int get_rate_rank(struct click_wifi_extra *ceh) {

    int try_cnt;

    if (ceh == NULL)
      click_chatter("BRNWifi::get_rate_rank: given pointer was NULL");


    try_cnt = ceh->max_tries;
    if (try_cnt >= ceh->retries)
      return 0;


    try_cnt = try_cnt + ceh->max_tries1;
    if (try_cnt >= ceh->retries)
      return 1;


    try_cnt = try_cnt + ceh->max_tries2;
    if (try_cnt >= ceh->retries)
      return 2;


    try_cnt = try_cnt + ceh->max_tries3;
    if (try_cnt >= ceh->retries)
      return 3;


    return -1;
  }


};

CLICK_ENDDECLS

#endif /* !_BRNWIFI_H_ */
