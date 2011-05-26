#ifndef BRN_DISCARD_HH
#define BRN_DISCARD_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include "elements/brn2/brnelement.hh"

CLICK_DECLS

/*
=c

BrnDiscard([I<keyword> ACTIVE])

=s basicsources

drops all packets

=d

BrnDiscards all packets received on its single input. If used in a pull context,
it initiates pulls whenever packets are available, and listens for activity
notification, such as that available from Queue.

Keyword arguments are:

=over 8

=item ACTIVE

Boolean.  If false, then BrnDiscard does not pull packets.  Default is true.
Only meaningful if BrnDiscard is in pull context.

=back

=h active rw

Returns or sets the ACTIVE parameter.  Only present if BrnDiscard is in pull
context.

=h count read-only

Returns the number of packets discarded.

=h reset_counts write-only

Resets "count" to 0.

=a Queue */

class BrnDiscard : public BRNElement { public:

    BrnDiscard();
    ~BrnDiscard();

    const char *class_name() const		{ return "BrnDiscard"; }
    const char *port_count() const		{ return PORTS_1_0; }
    const char *processing() const		{ return AGNOSTIC; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);
    void add_handlers();

    void push(int, Packet *);
    bool run_task(Task *);

  protected:

    Task _task;
    NotifierSignal _signal;

#if HAVE_INT64_TYPES
    typedef uint64_t counter_t;
#else
    typedef uint32_t counter_t;
#endif
    counter_t _count;

    bool _active;

    enum { h_reset_counts = 0, h_active = 1 };
    static int write_handler(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
