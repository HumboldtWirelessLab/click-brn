#ifndef RTSCTS_PACKETSIZE_HH
#define RTSCTS_PACKETSIZE_HH

#include <click/element.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>

#include "rtscts_scheme.hh"

CLICK_DECLS

class RtsCtsPacketSize: public RtsCtsScheme {
 public:
  RtsCtsPacketSize();
  ~RtsCtsPacketSize();

  const char *class_name() const { return "RtsCtsPacketSize"; }

  int configure(Vector<String> &conf, ErrorHandler* errh);

  bool set_rtscts(EtherAddress &dst, uint32_t size);
  uint32_t get_id() { return RTS_CTS_STRATEGY_SIZE_LIMIT;}

  uint32_t _psize;

};

CLICK_ENDDECLS

#endif
