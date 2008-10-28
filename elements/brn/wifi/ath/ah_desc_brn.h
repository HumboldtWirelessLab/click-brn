#ifndef _ATHDESCBRN_H_
#define _ATHDESCBRN_H_

struct ath_brn_rx_info {
  int8_t noise;
  u_int64_t hosttime;
  u_int64_t mactime;
};

struct ath_brn_tx_info {
  int8_t channel;
  u_int8_t mac[6];
};

struct ath_brn_info {
  u_int16_t id;
  union {
    struct ath_brn_rx_info rx;
    struct ath_brn_tx_info tx;
  } anno;
};

#define ATHDESC2_BRN_ID 0xF2F2

#endif
