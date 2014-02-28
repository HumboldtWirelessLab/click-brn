#ifndef RTSCTS_PLI_HH
#define RTSCTS_PLI_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include <elements/brn/wifi/rxinfo/packetlossestimator/packetlossinformation/packetlossinformation.hh>

#include "rtscts_scheme.hh"

CLICK_DECLS

class RtsCtsPLI: public RtsCtsScheme {
 public:
  RtsCtsPLI();
  ~RtsCtsPLI();

  const char *class_name() const { return "RtsCtsPLI"; }
  void *cast(const char *name);

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(PacketInfo *pinfo);
  bool handle_strategy(uint32_t strategy) { return (strategy == RTS_CTS_STRATEGY_PLI);}
  uint32_t get_max_strategy() { return RTS_CTS_STRATEGY_PLI;}

  PacketLossInformation *_pli; //PacketLossInformation-Element (see:../packetlossinformation/packetlossinformation.hh)

};

CLICK_ENDDECLS

#endif
