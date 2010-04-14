#ifndef FALCON_LEAVE_ORGANIZER_HH
#define FALCON_LEAVE_ORGANIZER_HH
#include <click/element.hh>

#include "elements/brn2/standard/md5.h"
#include "falcon_routingtable.hh"
CLICK_DECLS


class FalconLeaveOrganizer : public Element
{
  public:
    FalconLeaveOrganizer();
    ~FalconLeaveOrganizer();

/*ELEMENT*/
    const char *class_name() const  { return "FalconLeaveOrganizer"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

  private:
    FalconRoutingTable *_frt;

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);

    int _max_retries;

    int _debug;
};

CLICK_ENDDECLS
#endif
