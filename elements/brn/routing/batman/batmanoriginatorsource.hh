#ifndef BATMANORIGINATORSOURCEELEMENT_HH
#define BATMANORIGINATORSOURCEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"

#include "batmanroutingtable.hh"

CLICK_DECLS

class BatmanOriginatorSource : public BRNElement {

 public:

  class BrnBroadcast
  {
    public:
      uint16_t      bcast_id;
      uint8_t       _dst[6];
      uint8_t       _src[6];

      BrnBroadcast( uint16_t _id, uint8_t *src, uint8_t *dst )
      {
        bcast_id = _id;
        memcpy(&_src[0], src, 6);
        memcpy(&_dst[0], dst, 6);
      }

      ~BrnBroadcast()
      {}
  };

  //
  //methods
  //
  BatmanOriginatorSource();
  ~BatmanOriginatorSource();

  const char *class_name() const  { return "BatmanOriginatorSource"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void sendOriginator();
  void set_send_timer();

 private:
  //
  //member
  //
  BatmanRoutingTable *_brt;
  BRN2NodeIdentity *_nodeid;
  uint16_t _id;

  int _interval;

  Timer _send_timer;
  static void static_send_timer_hook(Timer *, void *);

};

CLICK_ENDDECLS
#endif
