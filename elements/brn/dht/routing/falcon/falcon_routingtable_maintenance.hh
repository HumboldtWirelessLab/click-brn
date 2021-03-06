#ifndef FALCON_ROUTINGTABLE_MAINTENANCE_HH
#define FALCON_ROUTINGTABLE_MAINTENANCE_HH
#include <click/element.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/standard/packetsendbuffer.hh"
#include "elements/brn/routing/hawk/hawk_routingtable.hh"

#include "falcon_routingtable.hh"

CLICK_DECLS

#define FALCON_DEFAULT_UPDATE_INTERVAL  2000
#define FALCON_DEFAULT_START_TIME      10000

#define FALCON_OPTIMAZATION_NONE               0
#define FALCON_OPTIMAZATION_FWD_TO_BETTER_SUCC 1
#define FALCON_OPT_SUCC_HINT 2
#define FALCON_OPT_FWD_SUCC_WITH_SUCC_HINT 3


class FalconRoutingTableMaintenance : public BRNElement
{
  public:
    FalconRoutingTableMaintenance();
    ~FalconRoutingTableMaintenance();

/*ELEMENT*/
    const char *class_name() const  { return "FalconRoutingTableMaintenance"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

  private:
    FalconRoutingTable *_frt;

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    void set_lookup_timer();

    void table_maintenance();

    void handle_request_pos(Packet *packet);
    void handle_reply_pos(Packet *packet);

    int _start;
    int _update_interval;

    int _rounds_to_passive_monitoring;
    int _current_round2pm;
    int _opti;
    HawkRoutingtable *_rfrt;

  public:
    void setHawkRoutingTable(HawkRoutingtable *t) { _rfrt = t; }

};

CLICK_ENDDECLS
#endif
