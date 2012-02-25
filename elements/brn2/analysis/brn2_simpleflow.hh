#ifndef CLICK_BRN2SIMPLEFLOW_HH
#define CLICK_BRN2SIMPLEFLOW_HH
#include <click/bighashmap.hh>
#include <click/dequeue.hh>
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/brn2.h"
#include "elements/brn2/brnelement.hh"


CLICK_DECLS

class BRN2SimpleFlow : public BRNElement
{
  CLICK_SIZE_PACKED_STRUCTURE(
  struct flowPacketHeader {,
    uint16_t crc;
    uint16_t reserved;

    uint8_t src[6];
    uint8_t dst[6];

    uint32_t flowID;
    uint32_t packetID;

    uint32_t rate;

    uint16_t size;
    uint8_t  mode;
    uint8_t  reply;

    uint32_t tv_sec;   /* seconds since 1.1.1970 */ //previous: unsigned long
    uint32_t tv_usec;  /* und microseconds */       //previous: long
  });

#define SIMPLEFLOW_MAXHOPCOUNT   100
#define MINIMUM_FLOW_PACKET_SIZE sizeof(struct flowPacketHeader)

  typedef enum flowType
  {
    TYPE_NO_ACK    = 0,
    TYPE_SMALL_ACK = 1,
    TYPE_FULL_ACK  = 2
  } FlowType;

 public:
  class Flow
  {
    public:
      EtherAddress _src;
      EtherAddress _dst;

      uint32_t _id;

      FlowType _type;

      uint32_t _rate;
      uint32_t _size;
      uint32_t _duration;

      bool _active;

      uint32_t _txPackets;
      uint32_t _rxPackets;

      uint32_t _rxCrcErrors;

      uint32_t _cum_sum_hops;
      uint32_t _min_hops;
      uint32_t _max_hops;
      uint32_t _sum_sq_hops;

      uint32_t _cum_sum_rt_time;
      uint32_t _min_rt_time;
      uint32_t _max_rt_time;
      uint32_t _sum_sq_rt_time;

      Timestamp _start_time;

      Flow() {}

      Flow(EtherAddress src, EtherAddress dst, int id, FlowType type, int rate, int size, int duration) {
        _src = src;
        _dst = dst;
        _id = id;
        _type = type;
        _rate = rate;
        _size = size;
        _duration = duration;
        _active = false;
        _txPackets = 0;
        _rxPackets = 0;
        _rxCrcErrors = 0;
        _cum_sum_hops = 0;
        _cum_sum_rt_time = 0;
        _min_hops = _max_hops = _sum_sq_hops = 0;
        _min_rt_time = _max_rt_time = _sum_sq_rt_time = 0;
      }

      ~Flow() {}

      void reset() {
        _active = false;
        _txPackets = 1;
        _rxPackets = 0;
        _rxCrcErrors = 0;
        _cum_sum_hops = _min_hops = _max_hops = _sum_sq_hops = 0;

        _min_rt_time = _max_rt_time = _sum_sq_rt_time =
        _cum_sum_rt_time = 0;
      }

      void add_rx_stats(uint32_t time, uint32_t hops) {
        _rxPackets++;

        _cum_sum_rt_time += time;
        if ( time > _max_rt_time ) _max_rt_time = time;
        if ( (time < _min_rt_time) || ( _min_rt_time == 0 ) ) _min_rt_time = time;
        _sum_sq_rt_time += (time*time);

        _cum_sum_hops += hops;
        if ( hops > _max_hops ) _max_hops = hops;
        if ( (hops < _min_hops) || ( _min_hops == 0 ) ) _min_hops = hops;
        _sum_sq_hops += (hops*hops);
      }

      uint32_t std_time() {
        if ( _rxPackets == 0 ) return 0;
        int32_t q = _cum_sum_rt_time/_rxPackets;
        return isqrt32((_sum_sq_rt_time/_rxPackets)-(q*q));
      }

      uint32_t std_hops() {
        if ( _rxPackets == 0 ) return 0;
        int32_t q = _cum_sum_hops/_rxPackets;
        return isqrt32((_sum_sq_hops/_rxPackets)-(q*q));
      }

  };

    Timer _timer;

    typedef HashMap<EtherAddress, Flow> FlowMap;
    typedef FlowMap::const_iterator FMIter;

    /*****************/
    /** M E M B E R **/
    /*****************/

    BRN2SimpleFlow();
    ~BRN2SimpleFlow();

/*ELEMENT*/
    const char *class_name() const  { return "BRN2SimpleFlow"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push(int port, Packet *packet);

    void add_handlers();

    void run_timer(Timer *t);

    void set_active(EtherAddress *dst, bool active);
    bool is_active(EtherAddress *dst);
    void schedule_next(EtherAddress *dst);

    void add_flow( EtherAddress src, EtherAddress dst,
                   uint32_t rate, uint32_t size, uint32_t mode,
                   uint32_t duration, bool active );

    void reset();

    FlowMap _rx_flowMap;
    FlowMap _tx_flowMap;

    EtherAddress dst_of_flow;

    String xml_stats();

  private:

    WritablePacket*  nextPacketforFlow(Flow *f);

    bool _clear_packet;
    int _headroom;

    bool _start_active;

    uint32_t _flow_id;
};

CLICK_ENDDECLS
#endif
