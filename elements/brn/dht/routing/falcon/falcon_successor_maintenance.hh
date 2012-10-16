#ifndef FALCON_SUCCESSOR_MAINTENANCE_HH
#define FALCON_SUCCESSOR_MAINTENANCE_HH
#include <click/element.hh>

#include "elements/brn2/standard/packetsendbuffer.hh"
#include "falcon_routingtable.hh"

#include "elements/brn2/routing/hawk/hawk_routingtable.hh"

CLICK_DECLS

#define FALCON_DEFAULT_SUCCESSOR_UPDATE_INTERVAL  2000
#define FALCON_DEFAULT_SUCCESSOR_MIN_PING            3
#define FALCON_DEFAULT_SUCCESSOR_START_TIME      10000

#define FALCON_OPTIMAZATION_FWD_TO_BETTER_SUCC 1

class FalconSuccessorMaintenance : public Element
{
  public:
    FalconSuccessorMaintenance();
    ~FalconSuccessorMaintenance();

/*ELEMENT*/
    const char *class_name() const  { return "FalconSuccessorMaintenance"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void handle_routing_update_callback(int status);

  private:
    FalconRoutingTable *_frt;

    DHTnodelist predNotifierNodes; //Nodes, which has to be notifier if my Pred, and so its succ changes

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    void set_lookup_timer();

    void successor_maintenance();

    void handle_request_succ(Packet *packet);
    void handle_reply_succ(Packet *packet, bool isUpdate);

    int _start;
    int _update_interval;

    int _min_successor_ping;
    int _debug;

    HawkRoutingtable *_rfrt;

    int _opti;

 public:
    void setHawkRoutingTable(HawkRoutingtable *t) { _rfrt = t; }
};

CLICK_ENDDECLS
#endif
