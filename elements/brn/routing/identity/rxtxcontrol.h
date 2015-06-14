#ifndef CLICK_RXTXCONTROL_H
#define CLICK_RXTXCONTROL_H

enum {
       GET_TX_INFO = 0,
       TX_ABORT = 1,
       SET_BACKOFFSCHEME = 2,
       CONFIG_BACKOFFSCHEME = 3 /*TODO: enable configuration of current bo_scheme*/
     };

enum {
       MAC_BACKOFF_SCHEME_DEFAULT = 0,
       MAC_BACKOFF_SCHEME_EXPONENTIAL = 1,
       MAC_BACKOFF_SCHEME_FIBONACCI = 2
     };

struct tx_control_header {
  uint8_t operation;
  uint8_t flags;

  union {
    uint8_t dst_ea[6];
    uint32_t bo_scheme;
    char bo_scheme_config[32];
  };
} __attribute__ ((packed));

#endif
