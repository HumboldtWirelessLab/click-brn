#ifndef CLICK_DHTROUTING_OMNISCIENT_HH
#define CLICK_DHTROUTING_OMNISCIENT_HH
#include <click/timer.hh>

#include "elements/brn/dht/md5.h"

#include "elements/brn/dht/standard/brn2_dhtnode.hh"
#include "elements/brn/dht/standard/brn2_dhtnodelist.hh"
#include "elements/brn/dht/routing/brn2_dhtrouting.hh"

CLICK_DECLS

class BRNLinkStat;

class DHTRoutingOmni : public DHTRouting
{
  public:
    DHTRoutingOmni();
    ~DHTRoutingOmni();

    class BufferedPacket
    {
      public:
        Packet *_p;
        struct timeval _send_time;

        BufferedPacket(Packet *p, int time_diff)
        {
          assert(p);
          _p=p;
          _send_time = Timestamp::now().timeval();
          _send_time.tv_sec += ( time_diff / 1000 );
          _send_time.tv_usec += ( ( time_diff % 1000 ) * 1000 );
          while( _send_time.tv_usec >= 1000000 )  //handle timeoverflow
          {
            _send_time.tv_usec -= 1000000;
            _send_time.tv_sec++;
          }
        }

        void check() const { assert(_p); }

    };

    typedef Vector<BufferedPacket> SendBuffer;

/*ELEMENT*/
    const char *class_name() const  { return "DHTRoutingOmni"; }

    void *cast(const char *name);

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "2/2"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

/*DHTROUTING*/
    const char *dhtrouting_name() const { return "DHTRoutingOmni"; }

    bool replication_support() const { return false; }
    int max_replication() const { return(1); }

  private:

    int _debug;
    BRNLinkStat *_linkstat;

    DHTnode _me;
    DHTnodelist _dhtnodes;


    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    void set_lookup_timer();
    void nodeDetection();

    int _update_interval;

};

CLICK_ENDDECLS
#endif
