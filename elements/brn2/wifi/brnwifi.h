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

#endif /* !_BRNWIFI_H_ */
