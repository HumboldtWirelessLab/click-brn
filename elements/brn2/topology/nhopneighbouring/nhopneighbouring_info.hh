#ifndef NHOPNEIGHBOURINGINFOELEMENT_HH
#define NHOPNEIGHBOURINGINFOELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brn2.h"
#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

#define DEFAULT_HOP_LIMIT       5

CLICK_DECLS

/*
 * TODO: max age for neighbours
 */

class NHopNeighbouringInfo : public BRNElement {

 public:

  class NeighbourInfo {
   public:
    EtherAddress _ea;

    uint8_t _hops;

    Timestamp _last_seen;

    NeighbourInfo() {
      _ea = ETHERADDRESS_BROADCAST;
      _hops = 0;
      _last_seen = Timestamp::now();
    }

    NeighbourInfo(const EtherAddress *ea, uint32_t hops) {
      _ea = *ea;
      _hops = hops;
      _last_seen = Timestamp::now();
    }

    void update(uint8_t hops, Timestamp last_seen) {
      _hops = hops;
      _last_seen = last_seen;
    }
  };

  typedef HashMap<EtherAddress, NeighbourInfo> NeighbourTable;
  typedef NeighbourTable::const_iterator NeighbourTableIter;

 public:
  //
  //methods
  //
  NHopNeighbouringInfo();
  ~NHopNeighbouringInfo();

  const char *class_name() const  { return "NHopNeighbouringInfo"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

 private:

  NeighbourTable _ntable;

  uint32_t _hop_limit;

 public:

  uint32_t get_hop_limit() { return _hop_limit; }
  uint32_t count_neighbours();
  uint32_t count_neighbours(uint32_t hops);

  bool is_neighbour(EtherAddress *ea);
  void add_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours);
  void update_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours);

  String print_stats();

};

CLICK_ENDDECLS
#endif
