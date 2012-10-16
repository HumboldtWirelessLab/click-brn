#ifndef GEORLINKPROBEHANDLERELEMENT_HH
#define GEORLINKPROBEHANDLERELEMENT_HH

#include "elements/brn/brnelement.hh"
#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/timer.hh>
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "geortable.hh"

CLICK_DECLS

class GeorLinkProbeHandler : public BRNElement {

 public:
  //
  //methods
  //
  GeorLinkProbeHandler();
  ~GeorLinkProbeHandler();

  const char *class_name() const  { return "GeorLinkProbeHandler"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int lpSendHandler(char *buffer, int size, const EtherAddress *ea);
  int lpReceiveHandler(char *buffer, int size, EtherAddress *ea);

  String get_info();

 private:
  //
  //member
  //

  BRN2LinkStat *_linkstat;

 public:
  GeorTable *_rt;

};

CLICK_ENDDECLS
#endif
