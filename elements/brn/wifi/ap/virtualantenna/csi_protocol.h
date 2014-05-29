#ifndef CSI_PROTOCOL_H
#define CSI_PROTOCOL_H

/* Copy from bf_to_eff.h */

/* 3 different SISO configs = 3
 * 3 different MIMO2 configs = 3
 * 1 MIMO3 config = 1
 * total = 7 */
#define NUM_RATES_SISO   3
#define NUM_RATES_MIMO2  3
#define NUM_RATES_MIMO3  1
#define MAX_NUM_RATES    (NUM_RATES_SISO + NUM_RATES_MIMO2 + NUM_RATES_MIMO3)
#define FIRST_MIMO2      (NUM_RATES_SISO)
#define FIRST_MIMO3      (FIRST_MIMO2 + NUM_RATES_MIMO2)

struct csi_header {
  uint32_t flags;
  uint16_t bfee_payload_size;
  uint16_t eff_snr_size;
} __attribute__ ((packed));


struct csi_node {
  uint32_t csi_node_addr; //struct sockaddr_in
  uint32_t tx_node_addr;

  int id;
} __attribute__ ((packed));

/* copy from iwl_struct */
struct csi_iwl5000_bfee_notif {
  uint8_t  reserved;
  int8_t   noiseA, noiseB, noiseC;
  uint16_t bfee_count;
  uint16_t reserved1;
  uint8_t  Nrx, Ntx;
  uint8_t  rssiA, rssiB, rssiC;
  int8_t   noise;
  uint8_t  agc, antenna_sel;
  uint16_t len;
  uint16_t fake_rate_n_flags;
} __attribute__ ((packed));


struct csi_cn_msg {
  uint32_t id_idx;
  uint32_t id_val;

  uint32_t seq;
  uint32_t ack;

  uint16_t len;              /* Length of the following data */
  uint16_t flags;
} __attribute__ ((packed));


struct csi_packet {

  struct csi_header             header;
  struct csi_node               node;
  struct csi_iwl5000_bfee_notif bfee;
  struct csi_cn_msg             cn_msg;
  uint32_t eff_snrs_int[MAX_NUM_RATES][4];

} __attribute__ ((packed));

#endif
