#ifndef CLICK_CALRADIOENCAP_HH
#define CLICK_CALRADIOENCAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
CalradioEncap()

=d

=a

*/

class CalradioEncap : public Element {

  public:

    CalradioEncap();
    ~CalradioEncap();

    const char *class_name() const	{ return "CalradioEncap"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

  private:
    int _debug;

};

CLICK_ENDDECLS
#endif
