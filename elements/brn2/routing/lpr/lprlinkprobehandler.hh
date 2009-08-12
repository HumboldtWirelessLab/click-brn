#ifndef LPRLINKPROBEHANDLERELEMENT_HH
#define LPRLINKPROBEHANDLERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

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

 private:
  //
  //member
  //

  Brn2LinkTable *_lt;
  BRN2LinkStat *_linkstat;

 public:
  int _debug;

};

CLICK_ENDDECLS
#endif
