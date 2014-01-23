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

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(EtherAddress &dst, uint32_t size);
  uint32_t get_id() { return RTS_CTS_STRATEGY_PLI;}

  PacketLossInformation *_pli; //PacketLossInformation-Element (see:../packetlossinformation/packetlossinformation.hh)

};

CLICK_ENDDECLS

#endif
