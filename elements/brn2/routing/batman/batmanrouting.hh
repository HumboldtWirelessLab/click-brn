#ifndef BATMANROUTINGELEMENT_HH
#define BATMANROUTINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

CLICK_DECLS
/*
 * =c
 * BatmanRouting()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNBroadcastRouting-Packets
 * Output 0 : BRNBroadcastRouting-Packets
 * Output 1 : Packets to local
  * =d
 */
#define MAX_QUEUE_SIZE  1500

class BatmanRouting : public Element {

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
  BatmanRouting();
  ~BatmanRouting();

  const char *class_name() const  { return "BatmanRouting"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  Vector<BrnBroadcast> bcast_queue;
  uint16_t bcast_id;
  EtherAddress _my_ether_addr;

 public:
  int _debug;

};

CLICK_ENDDECLS
#endif
