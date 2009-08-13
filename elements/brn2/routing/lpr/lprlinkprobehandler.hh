#ifndef LPRLINKPROBEHANDLERELEMENT_HH
#define LPRLINKPROBEHANDLERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

CLICK_DECLS

class LPRLinkProbeHandler : public Element {

 public:
  //
  //methods
  //
  LPRLinkProbeHandler();
  ~LPRLinkProbeHandler();

  const char *class_name() const  { return "LPRLinkProbeHandler"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(char *buffer, int size);

  String get_info();

 private:
  void updateKnownHosts();
  void updateLinksToMe();
  //
  //member
  //

  BRN2LinkStat *_linkstat;
  class BRN2ETXMetric *_etx_metric;

  uint32_t _seq; //TODO: this just fakes a new seq-number for Linktable(ETX-etric). Try anotherway (check old seq,....) !!!

 public:
  int _debug;

  Vector<EtherAddress> known_hosts;
  unsigned char *known_links;
  unsigned char *known_timestamps;
  int max_hosts;
};

CLICK_ENDDECLS
#endif
