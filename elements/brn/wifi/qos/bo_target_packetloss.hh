#ifndef BO_TARGET_PACKETLOSS_HH
#define BO_TARGET_PACKETLOSS_HH

#include <click/element.hh>
#include <elements/brn/brnelement.hh>
#include <elements/brn/wifi/channelstats.hh>

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
  void add_handlers();

  /* BackoffScheme */
  uint16_t get_id();
  int get_cwmin(Packet *p, uint8_t tos);
  void handle_feedback(uint8_t retries);


public:
  BoTargetPacketloss();
  ~BoTargetPacketloss();


private:
  static const uint16_t _id               = 4;  // unique bo scheme identifier
  static const uint16_t _bo_start         = 63; // initial backoff

  ChannelStats *_cst;
  uint32_t _target_packetloss;
};



CLICK_ENDDECLS

#endif /* BO_TARGET_PACKETLOSS_HH */
