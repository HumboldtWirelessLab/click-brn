#ifndef FLOODINGLINKTABLEELEMENT_HH
#define FLOODINGLINKTABLEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

class FloodingLinktable : public BRNElement {

 public:
  //
  //methods
  //
  FloodingLinktable();
  ~FloodingLinktable();

  const char *class_name() const  { return "FloodingLinktable"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  BRN2LinkStat *_linkstat;
  Brn2LinkTable *_etxlinktable;
  Brn2LinkTable *_pdrlinktable;

  uint32_t etx_metric2pdr(uint32_t metric);

  int get_pdr(EtherAddress &src, EtherAddress &dst);

};

CLICK_ENDDECLS
#endif
