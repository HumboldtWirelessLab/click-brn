#ifndef BRNCORRELATEDDROP_HH
#define BRNCORRELATEDDROP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
 *=c
 *BRNCorrelatedDrop()
 *=s encapsulation, Ethernet
 *encapsulates packets in Ethernet header (information used from Packet::ether_header())
*/
class BRNCorrelatedDrop : public Element {

 public:
  //
  //methods
  //
  BRNCorrelatedDrop();
  ~BRNCorrelatedDrop();

  const char *class_name() const  { return "BRNCorrelatedDrop"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet *simple_action(Packet *);

  int _drop_pattern;
  int _packet;
  int _skew;
};

CLICK_ENDDECLS
#endif
