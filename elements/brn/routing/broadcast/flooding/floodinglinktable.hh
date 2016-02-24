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

#define FLOODING_LT_MODE_ETX       1
#define FLOODING_LT_MODE_PDR       2
#define FLOODING_LT_MODE_LINKSTATS 4
#define FLOODING_LT_MODE_LOCAL     8


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
  Brn2LinkTable *_locallinktable;

  uint32_t etx_metric2pdr(uint32_t metric);

  void get_neighbors(EtherAddress ethernet, Vector<EtherAddress> &neighbors);
  uint32_t get_link_metric(const EtherAddress &from, const EtherAddress &to);

  uint32_t get_link_pdr(const EtherAddress &src, const EtherAddress &dst);

  void print_all_metrics(const EtherAddress &src, const EtherAddress &dst);

  EtherAddress _linkstat_addr;

  uint32_t _pref_mode;
  uint32_t _used_mode;

};

CLICK_ENDDECLS
#endif
