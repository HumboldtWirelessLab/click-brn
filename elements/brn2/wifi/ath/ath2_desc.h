#ifndef _ATHDESCBRN_H_
#define _ATHDESCBRN_H_

#include "ieee80211_monitor_ath2.h"

#define HAL_TXSTAT_ALTRATE  0x80  /* alternate xmit rate used */
/* bits found in ts_status */
#define HAL_TXERR_XRETRY  0x01  /* excessive retries */
#define HAL_TXERR_FILT    0x02  /* blocked by tx filtering */
#define HAL_TXERR_FIFO    0x04  /* fifo underrun */
#define HAL_TXERR_XTXOP   0x08  /* txop exceeded */
#define HAL_TXERR_DESC_CFG_ERR  0x10  /* Error in 20/40 desc config */
#define HAL_TXERR_DATA_UNDERRUN 0x20  /* Tx buffer underrun */
#define HAL_TXERR_DELIM_UNDERRUN 0x40 /* Tx delimiter underrun */

/* bits found in ts_flags */
#define HAL_TX_BA   0x01  /* Block Ack seen */
#define HAL_TX_AGGR   0x02  /* Aggregate */

/* bits found in rs_status */
#define HAL_RXERR_CRC   0x01  /* CRC error on frame */
#define HAL_RXERR_PHY   0x02  /* PHY error, rs_phyerr is valid */
#define HAL_RXERR_FIFO    0x04  /* fifo overrun */
#define HAL_RXERR_DECRYPT 0x08  /* non-Michael decrypt error */
#define HAL_RXERR_MIC   0x10  /* Michael MIC decrypt error */

/* bits found in rs_flags */
#define HAL_RX_MORE   0x01  /* more descriptors follow */
#define HAL_RX_MORE_AGGR  0x02  /* more frames in aggr */
#define HAL_RX_GI   0x04  /* full gi */
#define HAL_RX_2040   0x08  /* 40 MHz */
#define HAL_RX_DELIM_CRC_PRE  0x10  /* crc error in delimiter pre */
#define HAL_RX_DELIM_CRC_POST 0x20  /* crc error in delim after */
#define HAL_RX_DECRYPT_BUSY 0x40  /* decrypt was too slow */
#define HAL_RX_DUP_FRAME  0x80  /* Dup frame rx'd on control channel */

enum {
  HAL_PHYERR_UNDERRUN   = 0,  /* Transmit underrun */
  HAL_PHYERR_TIMING   = 1,  /* Timing error */
  HAL_PHYERR_PARITY   = 2,  /* Illegal parity */
  HAL_PHYERR_RATE     = 3,  /* Illegal rate */
  HAL_PHYERR_LENGTH   = 4,  /* Illegal length */
  HAL_PHYERR_RADAR    = 5,  /* Radar detect */
  HAL_PHYERR_SERVICE    = 6,  /* Illegal service */
  HAL_PHYERR_TOR      = 7,  /* Transmit override receive */
  /* NB: these are specific to the 5212 */
  HAL_PHYERR_OFDM_TIMING    = 17, /* */
  HAL_PHYERR_OFDM_SIGNAL_PARITY = 18, /* */
  HAL_PHYERR_OFDM_RATE_ILLEGAL  = 19, /* */
  HAL_PHYERR_OFDM_LENGTH_ILLEGAL  = 20, /* */
  HAL_PHYERR_OFDM_POWER_DROP  = 21, /* */
  HAL_PHYERR_OFDM_SERVICE   = 22, /* */
  HAL_PHYERR_OFDM_RESTART   = 23, /* */
  HAL_PHYERR_CCK_TIMING   = 25, /* */
  HAL_PHYERR_CCK_HEADER_CRC = 26, /* */
  HAL_PHYERR_CCK_RATE_ILLEGAL = 27, /* */
  HAL_PHYERR_CCK_SERVICE    = 30, /* */
  HAL_PHYERR_CCK_RESTART    = 31, /* */
};

/* value found in rs_keyix to mark invalid entries */
#define HAL_RXKEYIX_INVALID ((u_int8_t) -1)
/* value used to specify no encryption key for xmit */
#define HAL_TXKEYIX_INVALID ((u_int) -1)

/* flags passed to tx descriptor setup methods */
#define HAL_TXDESC_CLRDMASK 0x0001  /* clear destination filter mask */
#define HAL_TXDESC_NOACK  0x0002  /* don't wait for ACK */
#define HAL_TXDESC_RTSENA 0x0004  /* enable RTS */
#define HAL_TXDESC_CTSENA 0x0008  /* enable CTS */
#define HAL_TXDESC_INTREQ 0x0010  /* enable per-descriptor interrupt */
#define HAL_TXDESC_VEOL   0x0020  /* mark virtual EOL */
/* NB: this only affects frame, not any RTS/CTS */
#define HAL_TXDESC_DURENA 0x0040  /* enable h/w write of duration field */
#define HAL_TXDESC_EXT_ONLY 0x0080  /* send on ext channel only (11n) */
#define HAL_TXDESC_EXT_AND_CTL  0x0100  /* send on ext + ctl channels (11n) */
#define HAL_TXDESC_VMF    0x0200  /* virtual more frag */

/* flags passed to rx descriptor setup methods */
#define HAL_RXDESC_INTREQ 0x0020  /* enable per-descriptor interrupt */

#endif
