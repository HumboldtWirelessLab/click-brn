#ifndef LPRLINKPROBEHANDLERELEMENT_HH
#define LPRLINKPROBEHANDLERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"


/**
 TODO: use timestamp to check wether a node out information to probe.
       e.g. Double link: man weiss bei double links, wenn nur der link A->B eingetragen wurde ob B->A
       nicht existiert (bzw. wieder kaput ist) oder der entsprechende knoten ihn nicht eingetragen hat.
       wenn der Timestamp auf 255 ist, so hat der Knoten noch nicht eingetregen, sondern ist in der Tabelle
       lediglich weil ain anderen ihn für den link braucht

       Use lprprotocol object to avoid several delete and malloc
*/

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

  bool _active;
};

CLICK_ENDDECLS
#endif
