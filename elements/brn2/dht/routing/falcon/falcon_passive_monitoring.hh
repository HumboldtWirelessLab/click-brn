#ifndef FALCON_PASSIVE_MONITORING_HH
#define FALCON_PASSIVE_MONITORING_HH
#include <click/element.hh>

#include "elements/brn2/brnelement.hh"

#include "falcon_routingtable.hh"

CLICK_DECLS

#define FALCON_PASSIVE_MONITORING_MODE_DEACTIVATED 0
#define FALCON_PASSIVE_MONITORING_MODE_REQUESTING  1
#define FALCON_PASSIVE_MONITORING_MODE_ACTIVATED   2
#define FALCON_PASSIVE_MONITORING_MODE_SIGNOFF     4

class FalconPassiveMonitoring : public BRNElement
{
  public:
    class MonitoredDHTNode {
      DHTnode _node;

      Timestamp _age;

      DHTnodelist _reverse_fingertable;
    };

    Vector<MonitoredDHTNode*> _dhtnodelist;

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

    void check_monitoring();

    void handle_leave_monitoring_activate(Packet *packet);
    void handle_leave_monitoring_deactivate(Packet *packet);
    void handle_node_failure(Packet *packet);
    void handle_node_update(Packet *packet);

    bool passive_monitoring_mode;

};

CLICK_ENDDECLS
#endif
