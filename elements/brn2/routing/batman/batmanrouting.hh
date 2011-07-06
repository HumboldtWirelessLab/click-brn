#ifndef BATMANROUTINGELEMENT_HH
#define BATMANROUTINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "batmanroutingtable.hh"


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

class BatmanRouting : public BRNElement {

 public:

  //
  //methods
  //
  BatmanRouting();
  ~BatmanRouting();

  const char *class_name() const  { return "BatmanRouting"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/3"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  BatmanRoutingTable *_brt;
  BRN2NodeIdentity *_nodeid;

  int _routeId;

};

CLICK_ENDDECLS
#endif
