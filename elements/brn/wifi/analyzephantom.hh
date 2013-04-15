#ifndef CLICK_ANALYZE_PHANTOM_HH
#define CLICK_ANALYZE_PHANTOM_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include "elements/brn/brnelement.hh"

CLICK_DECLS


class AnalyzePhantom : public BRNElement {
 public:

  AnalyzePhantom();
  ~AnalyzePhantom();

  const char *class_name() const  { return "AnalyzePhantom"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  Packet *simple_action(Packet *);
};


CLICK_ENDDECLS

#endif /* CLICK_ANALYZE_PHANTOM_HH */
