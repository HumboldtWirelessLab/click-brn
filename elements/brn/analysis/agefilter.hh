#ifndef CLICK_AGEFILTER_HH
#define CLICK_AGEFILTER_HH

#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class AgeFilter : public BRNElement { public:

    AgeFilter();
    ~AgeFilter();

    const char *class_name() const	{ return "AgeFilter"; }
    const char *port_count() const	{ return PORTS_1_1X2; }
    const char *processing() const	{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void add_handlers();

    Packet *kill(Packet *p);
    Packet *simple_action(Packet *);

  private:

    Timestamp maxage;

};

CLICK_ENDDECLS
#endif
