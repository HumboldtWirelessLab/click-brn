#ifndef _BRNWIFI_H_
#define _BRNWIFI_H_

enum {
  WIFI_EXTRA_RX_CRC_ERR	  = (1<<31), /* failed crc check */
  WIFI_EXTRA_RX_PHY_ERR   = (1<<30), /* failed phy check */
  WIFI_EXTRA_RX_FIFO_ERR   = (1<<29), /* failed fifo check */
  WIFI_EXTRA_RX_DECRYPT_ERR   = (1<<28), /* failed decrypt check */
  WIFI_EXTRA_RX_MIC_ERR   = (1<<27), /* failed mic check */
};

#endif /* !_BRNWIFI_H_ */
