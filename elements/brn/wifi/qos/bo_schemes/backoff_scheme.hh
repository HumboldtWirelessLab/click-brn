#ifndef BACKOFF_SCHEME_HH
#define BACKOFF_SCHEME_HH

#include <click/element.hh>
#include <click/packet.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/schemelist.hh"
#include "elements/brn/routing/identity/rxtxcontrol.h"

CLICK_DECLS


#define BACKOFF_STRATEGY_OFF                             0 /* default */
#define BACKOFF_STRATEGY_DIRECT                          1
#define BACKOFF_STRATEGY_MAX_THROUGHPUT                  2
#define BACKOFF_STRATEGY_BUSY_AWARE                      3
#define BACKOFF_STRATEGY_TARGET_PACKETLOSS               4
#define BACKOFF_STRATEGY_LEARNING                        5
#define BACKOFF_STRATEGY_TARGET_DIFF_RXTX_BUSY           6
#define BACKOFF_STRATEGY_NEIGHBOURS                      7
#define BACKOFF_STRATEGY_CONSTANT                        8
#define BACKOFF_STRATEGY_TX_AWARE                        9
#define BACKOFF_STRATEGY_FLOODING                       10
#define BACKOFF_STRATEGY_BUSY_TX_AWARE                  11
#define BACKOFF_STRATEGY_DIFF_RXTX_BUSY_TX_AWARE        12
#define BACKOFF_STRATEGY_MINSTREL                       13
#define BACKOFF_STRATEGY_MEDIUMSHARE                    14

#define BACKOFF_SCHEME_MIN_CWMIN                         1
#define BACKOFF_SCHEME_MAX_CWMAX                     65535

class BackoffScheme: public BRNElement, public Scheme {
public:
  BackoffScheme();
  virtual ~BackoffScheme();

  void *cast(const char *name);

  virtual int configure_phase() { return CONFIGURE_PHASE_PRIVILEGED; }

  virtual int get_cwmin(Packet *p, uint8_t tos) = 0;
  virtual void handle_feedback(uint8_t retries);

  virtual void set_conf(uint32_t min, uint32_t max);
  virtual void set_strategy(uint32_t strategy);

protected:
  uint32_t _min_cwmin;
  uint32_t _max_cwmin;
};



CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
