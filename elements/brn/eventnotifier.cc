#include <click/config.h>
#include "eventnotifier.hh"
CLICK_DECLS

void
EventNotifier::add_listener(EventListener *l)
{
    for (int i = 0; i < _listeners.size(); i++)
	if (_listeners[i] == l)
	    return;
    _listeners.push_back(l);
}

void
EventNotifier::remove_listener(EventListener *l)
{
    for (int i = 0; i < _listeners.size(); i++)
	if (_listeners[i] == l) {
	    _listeners[i] = _listeners.back();
	    _listeners.pop_back();
	    return;
	}
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(EventNotifier)
