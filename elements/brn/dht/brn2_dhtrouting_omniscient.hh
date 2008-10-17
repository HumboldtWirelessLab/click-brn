#ifndef CLICK_DHTROUTING_OMNISCIENT_HH
#define CLICK_DHTROUTING_OMNISCIENT_HH
#include <click/timer.hh>

#include "md5.h"

#include "brn2_dhtnode.hh"
#include "brn2_dhtnodelist.hh"
#include "brn2_dhtrouting.hh"

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

    int set_notify_callback(void *, void *);
    bool replication_support() const { return false; }
    int max_replication() const { return(1); }

  private:

    int _debug;
    BRNLinkStat *_linkstat;

    DHTnode _me;
    DHTnodelist _dhtnodes;
    DHTnodelist _dhtneighbors;

    Vector<EtherAddress> my_neighbors;

    Timer _lookup_timer;
    static void static_queue_timer_hook(Timer *, void *);
    void nodeDetection();

    Timer _sendbuffer_timer;
    static void static_lookup_timer_hook(Timer *, void *);
    SendBuffer packet_queue;
    int get_min_jitter_in_queue();
    void queue_timer_hook();
    unsigned int _min_dist;

    int _min_jitter,_jitter,_simulator,_update_interval;

    void sendNodeInfoToAll(uint8_t type);
    Packet *createInfoPacket( uint8_t type );

    void *_info_func;
    void *_info_obj;

    void set_lookup_timer();

};

CLICK_ENDDECLS
#endif
