#ifndef RTSCTS_SCHEME_HH
#define RTSCTS_SCHEME_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/schemelist.hh"
#include "elements/brn/wifi/brnavailablerates.hh"

CLICK_DECLS

class PacketInfo {
 public:
  EtherAddress _dst;

  uint32_t _p_size;
  struct click_wifi_extra *_ceh;

  MCS _rts_cts_rate;
  uint32_t _rts_cts_duration;
};

#define RTS_CTS_STRATEGY_NONE       0 /* default */
#define RTS_CTS_STRATEGY_ALWAYS_OFF 1 /* */
#define RTS_CTS_STRATEGY_ALWAYS_ON  2 /* */
#define RTS_CTS_STRATEGY_SIZE_LIMIT 3 /* unicast only */
#define RTS_CTS_STRATEGY_RANDOM     4 /* unicast only */
#define RTS_CTS_STRATEGY_PLI        5 /* unicast only */
#define RTS_CTS_STRATEGY_HIDDENNODE 6 /* unicast only */
#define RTS_CTS_STRATEGY_FLOODING   7

class RtsCtsScheme: public BRNElement, public Scheme {
public:
  RtsCtsScheme();
  virtual ~RtsCtsScheme();

  void *cast(const char *name);

  virtual void set_conf();
  virtual bool set_rtscts(PacketInfo *pinfo) = 0;
  virtual void handle_feedback(PacketInfo *pinfo);

};

CLICK_ENDDECLS

#endif /* BACKOFF_SCHEME_HH */
