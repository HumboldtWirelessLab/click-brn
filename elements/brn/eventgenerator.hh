#ifndef CLICK_EVENTGENERATOR_HH
#define CLICK_EVENTGENERATOR_HH
#include <click/element.hh>
#include "eventnotifier.hh"

CLICK_DECLS

class EventGenerator : public Element, public EventNotifier { public:

    EventGenerator();
    ~EventGenerator();

    const char *class_name() const	{ return "EventGenerator"; }
    const char *port_count() const	{ return PORTS_0_0; }
    
    //int configure_phase() const		{ return CONFIGURE_PHASE_FROMHOST; }
    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void *cast(const char *);
    
    void add_handlers();

  private:
  	static String read_handler(Element *, void *);
  	static int write_handler(const String &, Element *, void *, ErrorHandler *);
  
};

CLICK_ENDDECLS
#endif
