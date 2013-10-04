#ifndef CLICK_TXCONTROL_H
#define CLICK_TXCONTROL_H

enum { GET_TX_INFO = 0, TX_ABORT = 1};

struct tx_control_header {
  uint8_t operation;
  uint8_t flags;

  uint8_t dst_ea[6];
} __attribute__ ((packed));

#endif