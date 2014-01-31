#ifndef RTSCTS_HIDDENNODE_HH
#define RTSCTS_HIDDENNODE_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "rtscts_scheme.hh"

#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"
#include "elements/brn/wifi/rxinfo/channelstats/cooperativechannelstats.hh"

CLICK_DECLS

class RtsCtsHiddenNode: public RtsCtsScheme {
 public:
  RtsCtsHiddenNode();
  ~RtsCtsHiddenNode();

  const char *class_name() const { return "RtsCtsHiddenNode"; }
  void *cast(const char *name);

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(EtherAddress &dst, uint32_t size);
  bool handle_strategy(uint32_t strategy) { return (strategy == RTS_CTS_STRATEGY_HIDDENNODE);}

  HiddenNodeDetection *_hnd;
  CooperativeChannelStats *_cocst;

  bool _pessimistic;

};

CLICK_ENDDECLS

#endif
