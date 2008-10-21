#ifndef BRN2HOTSPOTSPACKETENCAPELEMENT_HH
#define BRN2HOTSPOTSAPCKETENCAPELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

/*
 * =c
 * BRN2HotSpotsPacketEncap()
 * =s
 * SEND Packet with given size
 * =d
 */

class BRN2HotSpotsPacketEncap : public Element {

 public:

  //
  //methods
  //

/* brn2_packetsource.cc**/

  BRN2HotSpotsPacketEncap();
  ~BRN2HotSpotsPacketEncap();

  const char *class_name() const  { return "BRN2HotSpotsPacketEncap"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1-3/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  int _debug;

};

CLICK_ENDDECLS
#endif
