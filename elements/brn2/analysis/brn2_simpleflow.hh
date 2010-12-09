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

      uint32_t _cum_sum_rt_time;

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
      }

      ~Flow() {}

      void reset() {
        _active = false;
        _txPackets = 1;
        _rxPackets = 0;
        _rxCrcErrors = 0;
        _cum_sum_hops = 0;
        _cum_sum_rt_time = 0;
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

    FlowMap _rx_flowMap;
    FlowMap _tx_flowMap;

    EtherAddress dst_of_flow;

  private:

    WritablePacket*  nextPacketforFlow(Flow *f);

    bool _clear_packet;
    int _headroom;

};

CLICK_ENDDECLS
#endif
