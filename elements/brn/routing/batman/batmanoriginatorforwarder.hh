#ifndef BATMANORIGINATORFORWARDERELEMENT_HH
#define BATMANORIGINATORFORWARDERELEMENT_HH

#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "batmanroutingtable.hh"

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_device.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

#define QUEUE_DELAY 10

class BatmanOriginatorForwarder : public BRNElement {

 public:

  class BatmanNeighbourInfo {
   public:
    uint32_t _originator_interval;

    uint32_t *_originator_ids;
    uint32_t _originator_ids_size;
    uint32_t _originator_ids_index;
    uint32_t _no_originator_ids;

    Timestamp _last_orignator_timestamp;

    uint8_t _rev_psr;
    Timestamp _last_rev_psr;

    BatmanNeighbourInfo(): _originator_ids(NULL) {
      _originator_ids = NULL;
    }

    BatmanNeighbourInfo(int originator_ids_size, uint32_t interval):
      _originator_ids(NULL),
      _originator_ids_size(0)
    {
      _originator_ids = (uint32_t*)malloc(originator_ids_size * sizeof(uint32_t));
      memset(_originator_ids, 0, originator_ids_size*sizeof(uint32_t));
      _originator_ids_size = originator_ids_size;
      _originator_ids_index = 0;
      _no_originator_ids = 0;
      _originator_interval = interval;
    }

    ~BatmanNeighbourInfo() {
      if ( _originator_ids != NULL ) {
        delete[] _originator_ids;
        _originator_ids = NULL;
      }
    }

    uint8_t get_fwd_psr(uint32_t duration, Timestamp &now) {

      uint32_t time_since_last = (now - _last_orignator_timestamp).msecval();
      if ( (time_since_last > duration) || ( _no_originator_ids == 0) ) return 0;

      uint32_t last_oid = _originator_ids[(_originator_ids_index+_originator_ids_size-1)%_originator_ids_size];
      uint32_t first_poss_oid = last_oid - ((duration-time_since_last)/_originator_interval);

      uint32_t rcv_org = 0;
      uint32_t first_oid = last_oid;

      while ((rcv_org <= _no_originator_ids) && (first_oid >= first_poss_oid) ) {
        rcv_org++;
        first_oid = _originator_ids[(_originator_ids_index+_originator_ids_size-rcv_org)%_originator_ids_size];
      }

      click_chatter("%d %d %d",first_oid,last_oid,rcv_org);
      return (100 * rcv_org)/(duration/_originator_interval);
    }

    void add_originator(uint32_t orig_id, Timestamp *now) {
      /* compare with last, add only if not equal */
      if ( _originator_ids[(_originator_ids_index+_originator_ids_size-1)%_originator_ids_size] != orig_id ) {
        _originator_ids_index = (_originator_ids_index + 1) % _originator_ids_size;
        _originator_ids[_originator_ids_index] = orig_id;
        _last_orignator_timestamp = *now;
        _no_originator_ids++;
      }
    }
  };

  typedef HashMap<EtherAddress,BatmanNeighbourInfo*> BatmanNeighbourInfoMap;
  typedef BatmanNeighbourInfoMap::const_iterator BatmanNeighbourInfoMapIter;

  //
  //methods
  //
  BatmanOriginatorForwarder();
  ~BatmanOriginatorForwarder();

  const char *class_name() const  { return "BatmanOriginatorForwarder"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  void run_timer(Timer *timer);

  String print_links();


  typedef Vector<Packet *> SendBuffer;

 private:
  //
  //member
  //
  BatmanRoutingTable *_brt;
  BRN2NodeIdentity *_nodeid;

  Timer _sendbuffer_timer;
  SendBuffer _packet_queue;

  BatmanNeighbourInfoMap _neighbour_map;

};

CLICK_ENDDECLS
#endif
