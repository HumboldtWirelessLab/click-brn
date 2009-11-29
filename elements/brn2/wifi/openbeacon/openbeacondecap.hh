#ifndef CLICK_OPENBEACONDECAP_HH
#define CLICK_OPENBEACONDECAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
OpenBeaconDecap()

=d

=a

*/

class OpenBeaconDecap : public Element {

  public:

    OpenBeaconDecap();
    ~OpenBeaconDecap();

    const char *class_name() const	{ return "OpenBeaconDecap"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

  private:
    int _debug;

};

CLICK_ENDDECLS
#endif
