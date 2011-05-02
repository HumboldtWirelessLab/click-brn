#ifndef _BRNWIFI_H_
#define _BRNWIFI_H_

enum {
  WIFI_EXTRA_RX_CRC_ERR	     = (1<<31), /* failed crc check */
  WIFI_EXTRA_RX_PHY_ERR      = (1<<30), /* failed phy check */
  WIFI_EXTRA_RX_FIFO_ERR     = (1<<29), /* failed fifo check */
  WIFI_EXTRA_RX_DECRYPT_ERR  = (1<<28), /* failed decrypt check */
  WIFI_EXTRA_RX_MIC_ERR      = (1<<27), /* failed mic check */
  WIFI_EXTRA_RX_ZERORATE_ERR = (1<<26), /* has zero rate */
  WIFI_EXTRA_SHORT_PREAMBLE  = (1<<25),
  WIFI_EXTRA_MCS_RATE        = (1<<24)
};


inline void
fromMCS(uint8_t bandwidth, uint8_t guard_interval, uint8_t fec_type, uint8_t mcs_index, uint8_t *rate) {
  *rate = ((bandwidth & 3) << 6) | ((guard_interval & 1) << 5) | ((fec_type & 1) << 4) | (mcs_index & 15);
}

inline void
toMCS(uint8_t *bandwidth, uint8_t *guard_interval, uint8_t *fec_type, uint8_t *mcs_index, uint8_t rate) {
  *mcs_index = rate & 15;
  *fec_type = ( rate & 16 ) >> 4;
  *guard_interval = ( rate & 32 ) >> 5;
  *bandwidth = ( rate & 192 ) >> 6;
}


inline int getMCSRate(uint8_t idx, uint8_t bw, uint8_t gi) {
	
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
	
	return ((idx>>3)+1)*rates[(idx&7)*4+bw*2+gi];

}


#endif /* !_BRNWIFI_H_ */
