#ifndef HAWK_ROUTINGTABLE_HH
#define HAWK_ROUTINGTABLE_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

#include "elements/brn2/dht/standard/dhtnode.hh"

CLICK_DECLS

#define HAWKMAXKEXCACHETIME 10000000

class HawkRoutingtable : public BRNElement {

 public:

  class RTEntry {
    public:
      int _dst_id_length;
      uint8_t _dst_id[MAX_NODEID_LENTGH];
      EtherAddress _dst;

      EtherAddress _next_phy_hop;
      EtherAddress _next_hop;


      Timestamp _time;

      RTEntry( EtherAddress *ea, uint8_t *id, int id_len,
               EtherAddress *next_phy, EtherAddress *next) {
        _dst_id_length = id_len;
        memcpy(_dst_id, id, MAX_NODEID_LENTGH);
        _dst = EtherAddress(ea->data());
        _next_hop = EtherAddress(next->data());
        _next_phy_hop = EtherAddress(next_phy->data());
        _time = Timestamp::now();
      }

      ~RTEntry() {};

      void update(DHTnode *n) {
        _dst_id_length = n->_digest_length;
        memcpy(_dst_id,n->_md5_digest, MAX_NODEID_LENTGH);
        _time = Timestamp::now();
      }

      void updateNextHop(EtherAddress *next) {
        _next_hop = EtherAddress(next->data());
      }

      void updateNextPhyHop(EtherAddress *next_phy) {
        _next_phy_hop = EtherAddress(next_phy->data());
      }

      bool nextHopIsNeighbour() {
        return memcmp(_next_phy_hop.data(), _next_hop.data(), 6) == 0;
      }
  };

  HawkRoutingtable();
  ~HawkRoutingtable();

  const char *class_name() const { return "HawkRoutingtable"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();

  String routingtable();

  Vector<RTEntry*> _rt;

  HawkRoutingtable::RTEntry *addEntry(EtherAddress *ea, uint8_t *id, int id_len,
                                      EtherAddress *next_phy);

  HawkRoutingtable::RTEntry *addEntry(EtherAddress *ea, uint8_t *id, int id_len,
                                      EtherAddress *next_phy, EtherAddress *next);

//  HawkRoutingtable::RTEntry *addLink(EtherAddress *dst, uint8_t *dst_id, int dst_len,
//                                     EtherAddress *src);

  RTEntry *getEntry(EtherAddress *ea);
  RTEntry *getEntry(uint8_t *id, int id_len);

  void delEntry(EtherAddress *ea);

  Vector<RTEntry*> *getRoute(EtherAddress *ea);

  bool isNeighbour(EtherAddress *ea);
  EtherAddress *getNextHop(EtherAddress *dst);
  bool hasNextPhyHop(EtherAddress *dst);


  RTEntry *getEntryForNextHop(EtherAddress *ea);

  /* Falcon element to register hawk_routingtable. Pointers are void to avoid including of specific
   * header-files which causes error. TODO: fix this. use specific pointers
   */
  void *_lprh;
  void *_succ_maint;
  void *_rt_maint;

};

CLICK_ENDDECLS
#endif
