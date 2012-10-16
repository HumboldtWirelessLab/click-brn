#ifndef CLICK_CALRADIODECAP_HH
#define CLICK_CALRADIODECAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
CalradioDecap()

=d

=a

*/

class CalradioDecap : public Element {

  public:

    CalradioDecap();
    ~CalradioDecap();

    const char *class_name() const	{ return "CalradioDecap"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

  private:
    int _debug;

};

CLICK_ENDDECLS
#endif
