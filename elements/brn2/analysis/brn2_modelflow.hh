#ifndef CLICK_BRN2MODELFLOW_HH
#define CLICK_BRN2MODELFLOW_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

CLICK_DECLS

class BRN2ModelFlow : public Element
{
  public:

    BRN2ModelFlow();
    ~BRN2ModelFlow();

/*ELEMENT*/
    const char *class_name() const  { return "BRN2ModelFlow"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "0/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    void run_timer(Timer *t);
    Timer _timer;

    void set_active();

    bool _active;

    int getNextPacketSize();
    int32_t getNextPacketTime();
    WritablePacket *nextPacket(int size);
  private:

    uint32_t packet_size_vector_len;
    uint16_t *packet_sizes;
    uint32_t packet_time_vector_len;
    uint32_t *interpacket_time;
};

CLICK_ENDDECLS
#endif