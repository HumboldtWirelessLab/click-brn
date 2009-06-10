#ifndef BATMANORIGINATORFORWARDERELEMENT_HH
#define BATMANORIGINATORFORWARDERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "batmanroutingtable.hh"

CLICK_DECLS
/*
 * =c
 * BatmanOriginatorForwarder()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNBroadcastRouting-Packets
 * Output 0 : BRNBroadcastRouting-Packets
 * Output 1 : Packets to local
  * =d
 */
#define MAX_QUEUE_SIZE  1500

class BatmanOriginatorForwarder : public Element {

 public:

  //
  //methods
  //
  BatmanOriginatorForwarder();
  ~BatmanOriginatorForwarder();

  const char *class_name() const  { return "BatmanOriginatorForwarder"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  EtherAddress _ether_addr;
  BatmanRoutingTable *_brt;

 public:

  int _debug;

};

CLICK_ENDDECLS
#endif
