#ifndef CLICK_CALRADIOSTATS_HH
#define CLICK_CALRADIOSTATS_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
CalradioStats()

=d

=a

*/

class CalradioStats : public Element {

  public:

    CalradioStats();
    ~CalradioStats();

    const char *class_name() const	{ return "CalradioStats"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

};

CLICK_ENDDECLS
#endif
