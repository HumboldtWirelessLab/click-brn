#ifndef CLICK_EVENTNOTIFIER_HH
#define CLICK_EVENTNOTIFIER_HH
#include <click/vector.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

// events
enum Events { NewNeighborEvent, RemovedNeighborEvent };

class Event { public:
	
	Event()					{ }
	~Event()				{ }
  
	Events _type;
};

class NewNeighbor : public Event { public:
	
	NewNeighbor(EtherAddress eth) {
		_type = NewNeighborEvent;
		_eth = eth;
	}
	~NewNeighbor()			{ }
	
  	EtherAddress _eth;
};

class RemovedNeighbor : public Event { public:
	
	RemovedNeighbor(EtherAddress eth) {
		_type = RemovedNeighborEvent;
		_eth = eth;
	}
	~RemovedNeighbor()			{ }
	
  	EtherAddress _eth;
};

class EventListener { public:

    EventListener()			{ }
    virtual ~EventListener()	{ }
	
    virtual void notify(Event) = 0;
};


class EventNotifier { public:

    EventNotifier()			{ }
    ~EventNotifier()		{ }

    void add_listener(EventListener *);
    void remove_listener(EventListener *);

    void notify(Event) const;
    
  private:

    Vector<EventListener *> _listeners;
    
};


inline void
EventNotifier::notify(Event e) const
{
    for (int i = 0; i < _listeners.size(); i++)
		_listeners[i]->notify(e);
}


CLICK_ENDDECLS
#endif
