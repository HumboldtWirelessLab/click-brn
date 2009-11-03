#ifndef RANDOMDELAYQUEUE_HH
#define RANDOMDELAYQUEUE_HH
#include <click/timer.hh>

#include "elements/brn2/standard/packetsendbuffer.hh"

CLICK_DECLS

class RandomDelayQueue : public Element
{
  public:
    RandomDelayQueue();
    ~RandomDelayQueue();

/*ELEMENT*/
    const char *class_name() const  { return "RandomDelayQueue"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();
    String delay_queue_info(void);

    static void static_lookup_timer_hook(Timer *, void *);

  private:

    PacketSendBuffer packetBuffer;
    int _debug;

    int _min_jitter, _jitter, _min_dist;

    Timer _sendbuffer_timer;
    void queue_timer_hook();

};

CLICK_ENDDECLS
#endif
