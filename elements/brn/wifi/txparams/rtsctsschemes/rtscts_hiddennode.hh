#ifndef RTSCTS_HIDDENNODE_HH
#define RTSCTS_HIDDENNODE_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "rtscts_scheme.hh"

#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"

CLICK_DECLS

class RtsCtsHiddenNode: public RtsCtsScheme {
 public:
  RtsCtsHiddenNode();
  ~RtsCtsHiddenNode();

  const char *class_name() const { return "RtsCtsHiddenNode"; }
  void *cast(const char *name);

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(PacketInfo *pinfo);

  HiddenNodeDetection *_hnd;
  bool _pessimistic;

};

CLICK_ENDDECLS

#endif
