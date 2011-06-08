#ifndef NHOPNEIGHBOURINGINFOEEWSELEMENT_HH
#define NHOPNEIGHBOURINGINFOEEWSELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brn2.h"
#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

#include "elements/brn2/services/sensor/gps/gps.hh"

#include "elements/brn2/services/sensor/alarmingprotocol/alarmingstate.hh"

#define DEFAULT_HOP_LIMIT       5

CLICK_DECLS

/*
 * TODO: max age for neighbours
 */

class NHopNeighbouringInfoEews : public BRNElement {

 public:

  class NeighbourInfoEews {
   public:
    EtherAddress _ea;

    uint8_t _hops;

    Timestamp _last_seen;

    GPSPosition _gpspos;

    uint8_t _state;

    NeighbourInfoEews() {
      _ea = ETHERADDRESS_BROADCAST;
      _hops = 0;
      _last_seen = Timestamp::now();
      _gpspos = GPSPosition(0, 0, 0);
      _state = STATE_NO_TRIGGER_NO_ALARM;
    }

//    NeighbourInfoEews(const EtherAddress *ea, uint32_t hops) {
//      _ea = *ea;
//      _hops = hops;
//      _last_seen = Timestamp::now();
//    }

    NeighbourInfoEews(const EtherAddress *ea, uint32_t hops, GPSPosition *gpspos, uint8_t state) {
      _ea = *ea;
      _hops = hops;
      _last_seen = Timestamp::now();
      _gpspos = *gpspos;
      _state = state;
    }


//    void update(uint8_t hops, Timestamp last_seen) {
//      _hops = hops;
//      _last_seen = last_seen;
//    }

    void update(uint8_t hops, Timestamp last_seen, GPSPosition *gpspos, uint8_t state) {
      _hops = hops;
      _last_seen = last_seen;
      _gpspos = *gpspos;
      _state = state;
    }

  };

  typedef HashMap<EtherAddress, NeighbourInfoEews> NeighbourTable;
  typedef NeighbourTable::const_iterator NeighbourTableIter;

 public:
  //
  //methods
  //
  NHopNeighbouringInfoEews();
  ~NHopNeighbouringInfoEews();

  const char *class_name() const  { return "NHopNeighbouringInfoEews"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

 private:

  NeighbourTable _ntable;

  uint32_t _hop_limit;

  AlarmingState *_as;



 public:

  uint32_t get_hop_limit() { return _hop_limit; }



  uint32_t count_neighbours();
  uint32_t count_neighbours(uint32_t hops);

  bool is_neighbour(EtherAddress *ea);
  void add_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours, GPSPosition *gpspos, uint8_t state);
  void update_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours, GPSPosition *gpspos, uint8_t state);


  String print_stats();

};

CLICK_ENDDECLS
#endif
