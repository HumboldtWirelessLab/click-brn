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

CLICK_DECLS

class BRN2SimpleFlow : public Element
{
  CLICK_SIZE_PACKED_STRUCTURE(
  struct flowPacketHeader {,
    uint8_t src[6];
    uint8_t dst[6];

    uint32_t flowID;
    uint32_t packetID;

    uint32_t rate;

    uint16_t size;
    uint8_t  mode;
    uint8_t  reply;

  });

#define MINIMUM_FLOW_PACKET_SIZE sizeof(struct flowPacketHeader)

  typedef enum flowType
  {
    TYPE_NO_ACK    = 0,
    TYPE_SMALL_ACK = 1,
    TYPE_FULL_ACK  = 2
  } FlowType;

  typedef enum flowDir
  {
    DIR_ME_SENDER   = 0,
    DIR_ME_RECEIVER = 1
  } FlowDir;

 public:
  class Flow
  {
    public:
      EtherAddress _src;
      EtherAddress _dst;

      uint32_t _id;

      FlowDir _dir;
      FlowType _type;

      uint32_t _rate;
      uint32_t _size;
      uint32_t _duration;

      bool _active;

      uint32_t _txPackets;
      uint32_t _rxPackets;

      Flow() {}

      Flow(EtherAddress src, EtherAddress dst, int id, FlowType type, FlowDir dir, int rate, int size, int duration) {
        _src = src;
        _dst = dst;
        _id = id;
        _type = type;
        _dir = dir;
        _rate = rate;
        _size = size;
        _duration = duration;
        _active = false;
        _txPackets = 0;
        _rxPackets = 0;
      }

      ~Flow() {}

  };

  public:

    Timer _timer;

    typedef HashMap<EtherAddress, Flow> FlowMap;
    typedef FlowMap::const_iterator FMIter;

    FlowMap _flowMap;

    BRN2SimpleFlow();
    ~BRN2SimpleFlow();

/*ELEMENT*/
    const char *class_name() const  { return "BRN2SimpleFlow"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    void add_handlers();

    void run_timer(Timer *t);

    void set_active();
    uint32_t get_txpackets(void) { return txFlow._txPackets; }
    EtherAddress *get_txdest(void) { return &txFlow._dst; }

    EtherAddress _src;
  private:

    WritablePacket*  nextPacketforFlow(Flow *f);
    Flow txFlow;

};

CLICK_ENDDECLS
#endif
