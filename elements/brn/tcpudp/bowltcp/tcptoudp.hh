#ifndef CLICK_TCPTOUDP_HH
#define CLICK_TCPTOUDP_HH
#include <click/element.hh>
#include <click/glue.hh>

CLICK_DECLS

class TCPtoUDP : public Element { public:

    TCPtoUDP() {}; 
    ~TCPtoUDP() {};

    const char *class_name() const 		{ return "TCPtoUDP"; }
    const char *port_count() const		{ return PORTS_1_1;  } 
    const char *processing() const		{ return AGNOSTIC;   }
    
    Packet *simple_action(Packet *); 
    
};

CLICK_ENDDECLS
#endif
