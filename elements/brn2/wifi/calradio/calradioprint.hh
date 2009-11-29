#ifndef CLICK_CALRADIOPRINT_HH
#define CLICK_CALRADIOPRINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
CalradioPrint()

=d

=a

*/

class CalradioPrint : public Element {

  public:

    CalradioPrint();
    ~CalradioPrint();

    const char *class_name() const	{ return "CalradioPrint"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

  private:
    int _debug;

};

CLICK_ENDDECLS
#endif
