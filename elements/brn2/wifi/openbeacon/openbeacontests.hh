#ifndef CLICK_OPENBEACONTESTS_HH
#define CLICK_OPENBEACONTESTS_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c
OpenBeaconTests()

=d

=a

*/

class OpenBeaconTests : public Element {

  public:

    OpenBeaconTests();
    ~OpenBeaconTests();

    const char *class_name() const	{ return "OpenBeaconTests"; }
    const char *port_count() const  { return "2/1"; }
    const char *processing() const  { return PUSH; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    void push(int port, Packet *);   
    
  private:
    int _debug;
    int _mode;
    int _count;
    String _label;
};

CLICK_ENDDECLS
#endif

