#ifndef CLICK_OPENBEACONPRINT_HH
#define CLICK_OPENBEACONPRINT_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
OpenBeaconPrint()

=d

=a

*/

class OpenBeaconPrint : public Element {

  public:

    OpenBeaconPrint();
    ~OpenBeaconPrint();

    const char *class_name() const	{ return "OpenBeaconPrint"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

  private:
    int _debug;
    String _label;
};

CLICK_ENDDECLS
#endif
