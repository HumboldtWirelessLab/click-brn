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
  void *cast(const char *name);

  int configure(Vector<String> &conf, ErrorHandler* errh);

  void add_handlers();

  bool set_rtscts(PacketInfo *pinfo);

  uint32_t _psize;
  uint32_t _pduration;

};

CLICK_ENDDECLS

#endif
