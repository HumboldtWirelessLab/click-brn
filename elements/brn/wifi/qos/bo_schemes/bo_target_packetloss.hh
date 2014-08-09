#ifndef BO_TARGET_PACKETLOSS_HH
#define BO_TARGET_PACKETLOSS_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>
#include <elements/brn/wifi/rxinfo/channelstats/channelstats.hh>

#include "backoff_scheme.hh"

CLICK_DECLS

class BoTargetPacketloss : public BackoffScheme {
/* Derived Functions */
 public:
  /* Element */
  const char *class_name() const  { return "BoTargetPacketloss"; }
  const char *processing() const  { return AGNOSTIC; }
  const char *port_count() const  { return "0/0"; }
  void *cast(const char *name);

  int configure(Vector<String> &, ErrorHandler *);

  /* BackoffScheme */
  int get_cwmin(Packet *p, uint8_t tos);

 public:
  BoTargetPacketloss();

 private:

  ChannelStats *_cst;
  uint32_t _target_packetloss;
};

CLICK_ENDDECLS

#endif /* BO_TARGET_PACKETLOSS_HH */
