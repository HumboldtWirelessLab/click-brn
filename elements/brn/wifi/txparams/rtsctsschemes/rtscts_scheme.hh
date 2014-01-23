#ifndef RTSCTS_SCHEME_HH
#define RTSCTS_SCHEME_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

#define RTS_CTS_STRATEGY_ALWAYS_OFF 0 /* default */
#define RTS_CTS_STRATEGY_ALWAYS_ON  1 /* */
#define RTS_CTS_STRATEGY_SIZE_LIMIT 2 /* unicast only */
#define RTS_CTS_STRATEGY_RANDOM     3 /* unicast only */
#define RTS_CTS_STRATEGY_PLI        4 /* unicast only */
#define RTS_CTS_STRATEGY_HIDDENNODE 5 /* unicast only */

class RtsCtsScheme: public BRNElement {
public:
  RtsCtsScheme();
  virtual ~RtsCtsScheme();

  virtual bool set_rtscts(EtherAddress &dst, uint32_t size) = 0;
  virtual uint32_t get_id() = 0;

  virtual void handle_feedback();

  virtual void set_conf();
  virtual void set_strategy(uint32_t strategy);

protected:
  uint32_t _strategy;
};



CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
