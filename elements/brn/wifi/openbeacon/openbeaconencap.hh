#ifndef CLICK_OPENBEACONENCAP_HH
#define CLICK_OPENBEACONENCAP_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
OpenBeaconEncap()

=d

=a

*/

class OpenBeaconEncap : public Element {

  public:

    OpenBeaconEncap();
    ~OpenBeaconEncap();

    const char *class_name() const	{ return "OpenBeaconEncap"; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    Packet *simple_action(Packet *);

  private:
    int _debug;
    ErrorHandler* _errh;
  
    uint8_t	opbecon_filter[6];
};

CLICK_ENDDECLS
#endif
