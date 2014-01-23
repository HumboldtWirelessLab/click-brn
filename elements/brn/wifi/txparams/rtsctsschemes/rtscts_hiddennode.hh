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

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(EtherAddress &dst, uint32_t size);
  uint32_t get_id() { return RTS_CTS_STRATEGY_HIDDENNODE;}

  HiddenNodeDetection *_hnd;
  CooperativeChannelStats *_cocst;

};

CLICK_ENDDECLS

#endif
