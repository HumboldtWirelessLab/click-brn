#include <click/config.h>
#include "eventgenerator.hh"
#include <click/confparse.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

CLICK_DECLS

EventGenerator::EventGenerator()
{
}

EventGenerator::~EventGenerator()
{
}

int
EventGenerator::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return 0;
}

int
EventGenerator::initialize(ErrorHandler *errh)
{
	return 0;
}


void *
EventGenerator::cast(const char *n)
{
    if (strcmp(n, "EventNotifier") == 0)
		return (EventNotifier *)this;
    else if (strcmp(n, "EventGenerator") == 0)
		return (Element *)this;
    else
		return Element::cast(n);
}

void
EventGenerator::cleanup(CleanupStage)
{
}

enum { ETHERADDRESS
};

String
EventGenerator::read_handler(Element *e, void *thunk)
{
    EventGenerator *ac = static_cast<EventGenerator *>(e);
    switch ((intptr_t)thunk) {
      case ETHERADDRESS:
	return "bla";
      default:
	return "<error>";
    }
}


int
EventGenerator::write_handler(const String &data, Element *e, void *thunk, ErrorHandler *errh)
{
    EventGenerator *ac = static_cast<EventGenerator *>(e);
    String s = cp_uncomment(data);
    switch ((intptr_t)thunk) {
      case ETHERADDRESS: {
	  EtherAddress val;
	  if (!cp_ethernet_address(s, &val))
	      return errh->error("argument to 'etheraddress' should be an ether address");
	  ac->notify(NewNeighbor(val));
	  return 0;
      }
      default:
	return errh->error("internal error");
    }
}

void
EventGenerator::add_handlers()
{
    add_write_handler("etheraddress", write_handler, (void *) ETHERADDRESS);
}


ELEMENT_REQUIRES(linuxmodule)
EXPORT_ELEMENT(EventGenerator)

CLICK_ENDDECLS
