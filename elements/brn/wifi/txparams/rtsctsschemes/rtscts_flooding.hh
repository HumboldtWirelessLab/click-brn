#ifndef RTSCTS_FLOODING_HH
#define RTSCTS_FLOODING_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "rtscts_scheme.hh"

#include "elements/brn/wifi/rxinfo/hiddennodedetection.hh"
#include "elements/brn/routing/broadcast/flooding/flooding.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_db.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"

CLICK_DECLS

class RtsCtsFlooding: public RtsCtsScheme {
 public:
  RtsCtsFlooding();
  ~RtsCtsFlooding();

  const char *class_name() const { return "RtsCtsFlooding"; }
  void *cast(const char *name);

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(PacketInfo *pinfo);

  BRN2NodeIdentity *_me;
  Flooding *_flooding;
  FloodingHelper *_fhelper;
  FloodingDB *_flooding_db;

  HiddenNodeDetection *_hnd;
  bool _pessimistic;

};

CLICK_ENDDECLS

#endif
