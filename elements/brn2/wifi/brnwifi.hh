#ifndef _BRNWIFI_HH_
#define _BRNWIFI_HH_

#include <click/config.h>
#include <clicknet/wifi.h>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

enum {
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
  WIFI_EXTRA_MCS_RATE0       = (1<<21)
};


#define WIFI_EXTRA_FLAG_MCS_RATE_START 21

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

class BrnWifi
{
 public:

  BrnWifi();
  ~BrnWifi();

  /* Single bit signals, whether rate is mcs-rate or "normal" rate */
  static inline uint8_t getMCS(struct click_wifi_extra *ceh, int index) {
    return (ceh->flags >> (WIFI_EXTRA_FLAG_MCS_RATE_START+index) & 1);
  }

  static inline void setMCS(struct click_wifi_extra *ceh, int index, uint8_t mcs) {
    ceh->flags |= (mcs & 1) << (WIFI_EXTRA_FLAG_MCS_RATE_START+index);
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
    memset((void*)ceh,0,sizeof(struct brn_click_wifi_extra_extention));
  }

  static inline struct brn_click_wifi_extra_extention *get_brn_click_wifi_extra_extention(Packet *p) {
    return (struct brn_click_wifi_extra_extention *)BRNPacketAnno::get_brn_wifi_extra_extention_anno(p);
  }

  static inline uint8_t getFEC(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] /*>> IEEE80211_FEC_INDEX*/) & 1);
  }

  static inline void setFEC(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t fec_type) {
    ceh->mcs_flags[index] |= (fec_type & 1) /*<< IEEE80211_FEC_INDEX*/;
  }

  static inline uint8_t getHTMode(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] >> IEEE80211_HT_MODE_INDEX) & 1);
  }

  static inline void setHTMode(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t htmode) {
    ceh->mcs_flags[index] |= (htmode & 1) << IEEE80211_HT_MODE_INDEX;
  }

  static inline uint8_t getPreambleLength(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] >> IEEE80211_PREAMBLE_LENGTH_INDEX) & 1);
  }

  static inline void setPreambleLength(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t pl) {
    ceh->mcs_flags[index] |= (pl & 1) << IEEE80211_PREAMBLE_LENGTH_INDEX;
  }

  static inline uint8_t getSTBC(struct brn_click_wifi_extra_extention *ceh, int index) {
    return ((ceh->mcs_flags[index] >> IEEE80211_STBC_INDEX) & 1);
  }

  static inline void setSTBC(struct brn_click_wifi_extra_extention *ceh, int index, uint8_t stbc) {
    ceh->mcs_flags[index] |= (stbc & 1) << IEEE80211_STBC_INDEX;
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

    return ( (mcs_index >> 3) + 1) * rates[ ((mcs_index & 7) << 2) + bandwidth << 1 + guard_interval ];

  }

  static inline int getRate(struct click_wifi_extra *ceh, int index) {
    if ( ceh->flags & ( 1 << (WIFI_EXTRA_FLAG_MCS_RATE_START+index)) ) {
      uint8_t mcs_index, bandwidth, guard_interval;

      switch (index) {
        case 0: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate);
        case 1: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate1);
        case 2: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate2);
        case 3: toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate3);
        default: return 0;
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

    return 0;
  }

};

CLICK_ENDDECLS

#endif /* !_BRNWIFI_H_ */
