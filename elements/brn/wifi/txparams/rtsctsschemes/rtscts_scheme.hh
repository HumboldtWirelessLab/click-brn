#ifndef RTSCTS_SCHEME_HH
#define RTSCTS_SCHEME_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

#define RTS_CTS_STRATEGY_NONE       0 /* default */
#define RTS_CTS_STRATEGY_ALWAYS_OFF 1 /* */
#define RTS_CTS_STRATEGY_ALWAYS_ON  2 /* */
#define RTS_CTS_STRATEGY_SIZE_LIMIT 3 /* unicast only */
#define RTS_CTS_STRATEGY_RANDOM     4 /* unicast only */
#define RTS_CTS_STRATEGY_PLI        5 /* unicast only */
#define RTS_CTS_STRATEGY_HIDDENNODE 6 /* unicast only */

class RtsCtsScheme: public BRNElement {
public:
  RtsCtsScheme();
  virtual ~RtsCtsScheme();

  virtual bool set_rtscts(EtherAddress &dst, uint32_t size) = 0;
  virtual bool handle_strategy(uint32_t strategy) = 0;

  virtual void handle_feedback();

  virtual void set_conf();
  virtual void set_strategy(uint32_t strategy);

protected:
  uint32_t _strategy;
};

CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
