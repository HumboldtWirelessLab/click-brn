#ifndef NHOPNEIGHBOURINGINFOELEMENT_HH
#define NHOPNEIGHBOURINGINFOELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"

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

  /* Used for the "Compressed Version" */
  class ForeignNeighbourInfo {
   public:
    EtherAddress _ea;
    uint8_t _hops;
    Timestamp _last_seen;

    Vector<EtherAddress> _his_neighbours;

    ForeignNeighbourInfo() {
      _ea = ETHERADDRESS_BROADCAST;
      _hops = 0;
      _last_seen = Timestamp::now();
    }

    ForeignNeighbourInfo(const EtherAddress *ea) {
      _ea = *ea;
      _hops = 0;
      _last_seen = Timestamp::now();
    }

    void update(uint8_t hops, Vector<EtherAddress> *his_neighbours) {
      _hops = hops;
      _his_neighbours.clear();

      for ( int i = 0; i < his_neighbours->size(); i++ ) {
        _his_neighbours.push_back((*his_neighbours)[i]);
      }
    }

    Vector<EtherAddress> *get_neighbours() {
      return &_his_neighbours;
    }
  };

  typedef HashMap<EtherAddress, ForeignNeighbourInfo> ForeignNeighbourTable;
  typedef ForeignNeighbourTable::const_iterator ForeignNeighbourTableIter;

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
  ForeignNeighbourTable _foreign_ntable;

  uint32_t _hop_limit;

 public:

  uint32_t get_hop_limit() { return _hop_limit; }
  uint32_t count_neighbours();
  uint32_t count_neighbours(uint32_t hops);
  uint32_t count_neighbours_max_hops(uint32_t hops);

  bool is_neighbour(EtherAddress *ea);
  void get_neighbours(Vector<EtherAddress> *nhop_neighbours, uint8_t hops);

  void add_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours);
  void update_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours);

  void update_foreign_neighbours(EtherAddress *ea, Vector<EtherAddress> *_neighbours, uint8_t hops);
  Vector<EtherAddress> *get_foreign_neighbours(EtherAddress *ea);

  void sort_etheraddress_list(Vector<EtherAddress> *_list);

  String print_stats();
  String print_neighbours_stats();


};

CLICK_ENDDECLS
#endif
