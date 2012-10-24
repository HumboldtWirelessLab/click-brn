#include <click/config.h>

#include "brnwifi.hh"

CLICK_DECLS



BrnWifi::BrnWifi()
{
}

BrnWifi::~BrnWifi()
{
}

/* Next is taken from linux-next/drivers/net/wireless/ath/ath9k/xmit.c */

static uint16_t bits_per_symbol[][2] = {
  /* 20MHz 40MHz */
  {    26,   54 },     /*  0: BPSK */
  {    52,  108 },     /*  1: QPSK 1/2 */
  {    78,  162 },     /*  2: QPSK 3/4 */
  {   104,  216 },     /*  3: 16-QAM 1/2 */
  {   156,  324 },     /*  4: 16-QAM 3/4 */
  {   208,  432 },     /*  5: 64-QAM 2/3 */
  {   234,  486 },     /*  6: 64-QAM 3/4 */
  {   260,  540 },     /*  7: 64-QAM 5/6 */
};

uint32_t
BrnWifi::pkt_duration(uint32_t pktlen, uint8_t rix, uint8_t width, uint8_t half_gi)
{
#define OFDM_PLCP_BITS          22
#define OFDM_SYMBOL_TIME        4
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#define L_STF                   8
#define L_LTF                   8
#define L_SIG                   4
#define HT_SIG                  8
#define HT_STF                  4
#define HT_LTF(_ns)             (4 * (_ns))
#define SYMBOL_TIME(_ns)        ((_ns) << 2) /* ns * 4 us */
#define SYMBOL_TIME_HALFGI(_ns) (((_ns) * 18 + 4) / 5)  /* ns * 3.6 us */
#define NUM_SYMBOLS_PER_USEC(_usec) (_usec >> 2)
#define NUM_SYMBOLS_PER_USEC_HALFGI(_usec) (((_usec*5)-4)/18)

  uint32_t nbits, nsymbits, duration, nsymbols;
  int streams;

  /* find number of symbols: PLCP + data */
  streams = HT_RC_2_STREAMS(rix);
  nbits = (pktlen << 3) + OFDM_PLCP_BITS;
  nsymbits = bits_per_symbol[rix % 8][width] * streams;
  nsymbols = (nbits + nsymbits - 1) / nsymbits;

  if (!half_gi)
    duration = SYMBOL_TIME(nsymbols);
  else
    duration = SYMBOL_TIME_HALFGI(nsymbols);

  /* addup duration for legacy/ht training and signal fields */
  duration += L_STF + L_LTF + L_SIG + HT_SIG + HT_STF + HT_LTF(streams);

  return duration;
}


CLICK_ENDDECLS

ELEMENT_PROVIDES(BrnWifi)
