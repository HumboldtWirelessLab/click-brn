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

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(EtherAddress &dst, uint32_t size);
  uint32_t get_id() { return RTS_CTS_STRATEGY_PLI;}

  uint32_t _prob;

};

CLICK_ENDDECLS

#endif
