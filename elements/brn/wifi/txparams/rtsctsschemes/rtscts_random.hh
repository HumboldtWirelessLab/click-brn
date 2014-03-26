#ifndef RTSCTS_RANDOM_HH
#define RTSCTS_RANDOM_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "rtscts_scheme.hh"

CLICK_DECLS

class RtsCtsRandom: public RtsCtsScheme {
 public:
  RtsCtsRandom();
  ~RtsCtsRandom();

  const char *class_name() const { return "RtsCtsRandom"; }
  void *cast(const char *name);

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(PacketInfo *pinfo);

  uint32_t _prob;

};

CLICK_ENDDECLS

#endif
