#ifndef FALCON_ROUTINGTABLE_MAINTENANCE_HH
#define FALCON_ROUTINGTABLE_MAINTENANCE_HH

#include "elements/brn2/standard/md5.h"
#include "elements/brn2/standard/packetsendbuffer.hh"

CLICK_DECLS

class FalconRoutingTableMaintenance : public Element
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

};

CLICK_ENDDECLS
#endif
