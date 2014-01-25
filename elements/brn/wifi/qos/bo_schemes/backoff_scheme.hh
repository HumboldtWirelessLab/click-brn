#ifndef BACKOFF_SCHEME_HH
#define BACKOFF_SCHEME_HH

#include <click/element.hh>
#include <click/packet.hh>

#include "elements/brn/brnelement.hh"

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

class BackoffScheme: public BRNElement {
public:
  BackoffScheme();
  virtual ~BackoffScheme();

  virtual bool handle_strategy(uint32_t strategy) = 0;
  virtual int get_cwmin(Packet *p, uint8_t tos) = 0;
  virtual void handle_feedback(uint8_t retries) = 0;

  virtual void set_conf(uint32_t min, uint32_t max);
  virtual void set_strategy(uint32_t strategy);

protected:
  uint32_t _min_cwmin;
  uint32_t _max_cwmin;
  uint32_t _strategy;
};



CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
