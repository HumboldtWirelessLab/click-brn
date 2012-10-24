#ifndef DART_ROUTINGTABLE_MAINTENANCE_HH
#define DART_ROUTINGTABLE_MAINTENANCE_HH
#include <click/element.hh>
#include <click/timer.hh>

#include "dart_routingtable.hh"
CLICK_DECLS

#define FALCON_DEFAULT_UPDATE_INTERVAL  2000
#define FALCON_DEFAULT_START_TIME      10000


class DartRoutingTableMaintenance : public Element
{
  public:
    DartRoutingTableMaintenance();
    ~DartRoutingTableMaintenance();

/*ELEMENT*/
    const char *class_name() const  { return "DartRoutingTableMaintenance"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

  private:
    DartRoutingTable *_drt;

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    void set_lookup_timer();

    void table_maintenance();

    void handle_request(Packet *packet);
    void handle_assign(Packet *packet);

  public:
    int _starttime;
    int _activestart;

  private:
    DHTnode* getBestNeighbour();
    void assign_id(DHTnode *newnode);

    int _update_interval;

  public:
    int _debug;
};

CLICK_ENDDECLS
#endif
