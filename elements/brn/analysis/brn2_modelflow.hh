#ifndef CLICK_BRN2MODELFLOW_HH
#define CLICK_BRN2MODELFLOW_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class BRN2ModelFlow : public BRNElement
{
  public:

    BRN2ModelFlow();
    ~BRN2ModelFlow();

/*ELEMENT*/
    const char *class_name() const  { return "BRN2ModelFlow"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    int getNextPacketSize();
    int32_t getNextPacketTime();

  private:

    uint32_t packet_size_vector_len;
    uint16_t *packet_sizes;
    uint32_t packet_time_vector_len;
    uint32_t *interpacket_time;
};

CLICK_ENDDECLS
#endif
