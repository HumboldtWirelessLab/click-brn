#ifndef CLICK_BRN2SIMPLEFLOW_HH
#define CLICK_BRN2SIMPLEFLOW_HH
#include <click/bighashmap.hh>
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

#include "elements/brn/routing/routing_peek.hh"

CLICK_DECLS

//#define SF_TX_ABORT

class FlowID {

  public:
    //
    //member
    //
    EtherAddress _src;
    uint32_t      _id;
    size_t  _hashcode;

    //
    //methods
    //
    FlowID() : _src(), _id(0), _hashcode(0) { }

    FlowID(EtherAddress src, uint32_t id) {
      _src = src;
      _id = id;
      _hashcode = (((uint32_t)_src.sdata()[2]) << 16) + _id;
    }

    inline bool operator==(FlowID other) {
      if ( _hashcode != other._hashcode ) return false;
      return (other._id == _id && other._src == _src);
    }
};

inline unsigned hashcode(FlowID fid) {
  return fid._hashcode;
}

class BRN2SimpleFlow : public BRNElement
{
  CLICK_SIZE_PACKED_STRUCTURE(
  struct flowPacketHeader {,
    uint16_t crc;
    uint8_t  flags;
    uint8_t  simpleflow_id;

    uint8_t src[6];
    uint8_t dst[6];

    uint32_t flowID;
    uint32_t packetID;

    uint16_t interval; //in ms -> max 65 sec between 2 packets
    uint16_t burst;

    uint16_t size;
    uint8_t  mode;
    uint8_t  extra_data_size;

    uint32_t tv_sec;   /* seconds since 1.1.1970 */ //previous: unsigned long
    uint32_t tv_usec;  /* und microseconds */       //previous: long
  });

#define SIMPLEFLOW_FLAGS_INCL_ROUTE (uint8_t)1
#define SIMPLEFLOW_FLAGS_IS_REPLY   (uint8_t)2

  CLICK_SIZE_PACKED_STRUCTURE(
  struct flowPacketRoute {,
    uint16_t hops;
    uint16_t incl_hops;
    uint32_t metric_sum;
  });

  CLICK_SIZE_PACKED_STRUCTURE(
  struct flowPacketHop {,
    uint16_t metric;
    uint8_t  ea[6];
  });

#define SIMPLEFLOW_MAXHOPCOUNT   100
#define MINIMUM_FLOW_PACKET_SIZE (sizeof(struct flowPacketHeader)+sizeof(struct click_brn))

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

      uint32_t _interval;
      uint32_t _size;
      uint32_t _burst;
      uint32_t _duration;

      bool _active;

      uint32_t _txPackets;
      uint32_t _tx_packets_feedback;

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
      Timestamp _end_time;
      Timestamp _next_time;

      Timestamp _first_feedback_time;
      Timestamp _last_feedback_time;

      uint32_t _max_packet_id;
      uint32_t _rx_out_of_order;

      Vector<EtherAddress> route;
      Vector<uint16_t> metric;

      uint32_t metric_sum;
      uint16_t route_hops;

      Packet *_buffered_p;

      String _extra_data;
      int8_t _header_size;

      Flow(): _type(TYPE_NO_ACK), _active(false), _buffered_p(NULL) {}

      Flow(EtherAddress src, EtherAddress dst, int id, FlowType type, int size, int interval, int burst, int duration) {
        _buffered_p = NULL;
        reset();
        _src = src;
        _dst = dst;
        _id = id;
        _type = type;
        _size = size;
        _interval = interval;
        _burst = burst;
        _duration = duration;
        _active = false;
        _start_time = Timestamp::now();
        _end_time = _start_time + Timestamp::make_msec(duration/1000, duration%1000);
      }

      ~Flow() {}

      void reset() {
        _active = false;
        _txPackets = _tx_packets_feedback = 0;
        _rxPackets = _rxCrcErrors = 0;
        _cum_sum_hops = _min_hops = _max_hops = _sum_sq_hops = 0;

        _min_rt_time = _max_rt_time = _sum_sq_rt_time = _cum_sum_rt_time = 0;
        _max_packet_id = _rx_out_of_order = 0;

        route.clear();
        metric.clear();

        metric_sum = route_hops = 0;

        if (_buffered_p != NULL) _buffered_p->kill();
        _buffered_p = NULL;

        _extra_data = "";
        _header_size = 0;
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

      //Bits per seconds
      uint32_t get_txfeedback_rate() {
        uint64_t duration = (uint64_t)((_last_feedback_time - _first_feedback_time).msecval());
        if ( duration == 0 ) return 0;

        uint64_t bits = (uint64_t)_tx_packets_feedback * (uint64_t)8000;
        if ( _header_size == -1 ) bits *= ((uint64_t)_size);
        else bits *= ((uint64_t)_size + _header_size);

        return (uint32_t)(bits/duration); // 8000 = 8 bit/byte * 1000ms/s
      }

      uint32_t get_rx_rate() {
        uint64_t duration = (uint64_t)((_end_time - _start_time).msecval());
        if ( duration == 0 ) return 0;

        uint64_t bits = (uint64_t)_rxPackets * (uint64_t)8000;
        if ( _header_size == -1 ) bits *= ((uint64_t)_size);
        else bits *= ((uint64_t)_size + _header_size);

        return (uint32_t)(bits/duration); // 8000 = 8 bit/byte * 1000ms/s
      }

  };

    Timer _timer;

    typedef HashMap<FlowID, Flow*> FlowMap;
    typedef FlowMap::const_iterator FMIter;

    /*****************/
    /** M E M B E R **/
    /*****************/

    BRN2SimpleFlow();
    ~BRN2SimpleFlow();

/*ELEMENT*/
    const char *class_name() const  { return "BRN2SimpleFlow"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1-2/1"; }

    int configure_phase() const { return CONFIGURE_PHASE_LAST; }
    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push(int port, Packet *packet);

    void add_handlers();

    void run_timer(Timer *t);

    void set_active(Flow *f, bool active);
    bool is_active(Flow *f);
    void schedule_next();

    void add_flow(EtherAddress src, EtherAddress dst,
                  uint32_t size, uint32_t mode,
                  uint32_t interval, uint32_t burst,
                  uint32_t duration, bool active,
                  uint32_t start_delay);
    void add_flow(EtherAddress src, EtherAddress dst,
                  uint32_t size, uint32_t mode,
                  uint32_t interval, uint32_t burst,
                  uint32_t duration, bool active,
                  uint32_t start_delay, String extra_data);

    void add_flow(String flow_conf);

    bool handle_routing_peek(Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port);

    void reset();

    FlowMap _rx_flowMap;
    FlowMap _tx_flowMap;

    String xml_stats();

 private:

    void send_packets_for_flow(Flow *fl);
    WritablePacket* nextPacketforFlow(Flow *f);

    void handle_reuse(Packet *packet);

    String _extra_data;
    bool _clear_packet;
    int32_t _headersize;
    int _headroom;
    uint32_t _flow_id;
    uint32_t _simpleflow_element_id;

    RoutingPeek *_routing_peek;
    Brn2LinkTable *_link_table;

    uint32_t _flow_start_rand;

    Timestamp _next_schedule;

#ifdef SF_TX_ABORT
    void abort_transmission(EtherAddress &dst);
#endif

    String _init_flow;
};

CLICK_ENDDECLS
#endif
