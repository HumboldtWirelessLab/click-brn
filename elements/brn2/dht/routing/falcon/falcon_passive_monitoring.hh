#ifndef FALCON_PASSIVE_MONITORING_HH
#define FALCON_PASSIVE_MONITORING_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

#include "falcon_routingtable.hh"

CLICK_DECLS

#define FALCON_LEAVE_MODE_IDLE  0
#define FALCON_LEAVE_MODE_LEAVE 1

class FalconPassiveMonitoring : public BRNElement
{
  public:
    FalconPassiveMonitoring();
    ~FalconPassiveMonitoring();

/*ELEMENT*/
    const char *class_name() const  { return "FalconPassiveMonitoring"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

  private:
    FalconRoutingTable *_frt;

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);

    void handle_leave_monitoring_activate(Packet *packet);
    void handle_leave_monitoring_deactivate(Packet *packet);

};

CLICK_ENDDECLS
#endif
