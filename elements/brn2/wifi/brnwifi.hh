#ifndef _BRNWIFI_HH_
#define _BRNWIFI_HH_

#include <click/config.h>
#include <clicknet/wifi.h>

CLICK_DECLS

enum {
  WIFI_EXTRA_RX_CRC_ERR	     = (1<<31), /* failed crc check */
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
  WIFI_EXTRA_MCS_FEC3_LDPC   = (1<<20),
  WIFI_EXTRA_MCS_FEC2_LDPC   = (1<<19),
  WIFI_EXTRA_MCS_FEC1_LDPC   = (1<<18),
  WIFI_EXTRA_MCS_FEC0_LDPC   = (1<<17)
};

#define WIFI_EXTRA_FLAG_MCS_RATE_START 21
#define WIFI_EXTRA_FLAG_MCS_FEC_START  17


/*
-------------------------------------------------------------------
|   0   |   1   |   2   |   3   |   4   |   5   |   6  |    7     |
|                MCS INDEX              |   BANDWIDTH  | GUARDINT |
-------------------------------------------------------------------
*/

/*
    * max int > 2 000 000 000
    * split to 000.000000
    * max.  ~  999.999999
*/

#define PREFACTOR 1000000 /*6 digests*/

class BrnWifi
{
 public:

  BrnWifi();
  ~BrnWifi();

  static inline void fromMCS(uint8_t mcs_index, uint8_t bandwidth, uint8_t guard_interval, uint8_t *rate) {
    *rate = (mcs_index & 31) | ((bandwidth & 3) << 5) | ((guard_interval & 1) << 7);
  }

  static inline void toMCS(uint8_t *mcs_index, uint8_t *bandwidth, uint8_t *guard_interval, uint8_t rate) {
    *mcs_index = rate & 31;
    *bandwidth = (rate & 96) >> 5;
    *guard_interval = (rate & 128) >> 7;
  }

  static inline uint8_t getFEC(struct click_wifi_extra *ceh, int index) {
    return (ceh->flags >> (WIFI_EXTRA_FLAG_MCS_FEC_START+index) & 1);
  }

  static inline void setFEC(struct click_wifi_extra *ceh, int index, uint8_t fec_type) {
    ceh->flags |= fec_type << (WIFI_EXTRA_FLAG_MCS_FEC_START+index);
  }

  static inline uint8_t getMCS(struct click_wifi_extra *ceh, int index) {
    return (ceh->flags >> (WIFI_EXTRA_FLAG_MCS_RATE_START+index) & 1);
  }

  static inline void setMCS(struct click_wifi_extra *ceh, int index, uint8_t mcs) {
    ceh->flags |= mcs << (WIFI_EXTRA_FLAG_MCS_RATE_START+index);
  }

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
