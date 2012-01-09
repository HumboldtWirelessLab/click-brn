#ifndef BATMANROUTINGTABLEELEMENT_HH
#define BATMANROUTINGTABLEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinktable.hh"

CLICK_DECLS

#define BATMAN_ORIGINATORMODE_FWD        0
#define BATMAN_ORIGINATORMODE_COMPRESSED 1

class BatmanRoutingTable : public BRNElement {

 Timestamp now;

 void set_time_now() { now = Timestamp::now(); };

 public:

  class BatmanForwarderEntry
  {
    public:
      EtherAddress _forwarder;

      uint8_t _hops;
      uint32_t _metric_fwd_src;
      uint32_t _metric_to_fwd;
      uint32_t _metric;

      uint32_t _last_originator_id;

      BatmanForwarderEntry() {
        _hops = _metric_fwd_src = _metric_to_fwd = _metric = _last_originator_id = 0;
      }

      ~BatmanForwarderEntry() {}

      BatmanForwarderEntry(EtherAddress addr) {
        _forwarder = addr;
        _hops = _metric_fwd_src = _metric_to_fwd = _metric = _last_originator_id = 0;
      }

      void update_entry(uint32_t originator_id, uint8_t hops, uint32_t metric_fwd_src, uint32_t metric_to_fwd) {
        _last_originator_id = originator_id;
        _hops = hops;
        _metric_fwd_src = metric_fwd_src;
        _metric_to_fwd = metric_to_fwd;
        _metric = _metric_fwd_src + _metric_to_fwd;
      }
  };

  typedef HashMap<EtherAddress,BatmanForwarderEntry> BatmanForwarderMap;
  typedef BatmanForwarderMap::const_iterator BatmanForwarderMapIter;

  class BatmanNode
  {
   public:
    EtherAddress _addr;
    uint32_t _originator_interval;
    uint32_t _latest_originator_id;

    Timestamp _latest_originator_time;

    uint32_t _hops_best_metric;
    uint32_t _best_metric;
    EtherAddress _best_forwarder;

    BatmanForwarderMap _forwarder;

    Timestamp _last_forward_time;
    uint32_t  _last_forward_originator_id;

    BatmanNode() {}

    BatmanNode(EtherAddress addr) {
      _addr = addr;
      reset();
    }

    ~BatmanNode() {
      reset();
    }

    void reset() {
      _forwarder.clear();

      _forwarder.clear();
      _originator_interval = 0;
      _latest_originator_id = 0x0;
      _latest_originator_time = Timestamp::now();

      _best_forwarder = EtherAddress();
      _hops_best_metric = 0xffffffff;
      _best_metric = 0xffffffff;

      _last_forward_originator_id = click_random();
    }

    BatmanForwarderEntry *getBatmanForwarderEntry(EtherAddress addr) {
      return _forwarder.findp(addr);
    }

    BatmanForwarderEntry *addBatmanForwarderEntry(EtherAddress addr) {
      _forwarder.insert(addr,BatmanForwarderEntry(addr));
      return _forwarder.findp(addr);
    }

    void update_originator(uint32_t id, EtherAddress fwd_ea, uint32_t metric_fwd_src, uint32_t metric_to_fwd, uint8_t hops )
    {
      BatmanForwarderEntry *fwd = getBatmanForwarderEntry(fwd_ea);
      if ( fwd == NULL )
        fwd = addBatmanForwarderEntry(fwd_ea);

      if ( fwd->_last_originator_id > id ) {
        reset();
      }

      if ( id > _latest_originator_id ) {
        _latest_originator_id = id;
        _latest_originator_time = Timestamp::now();
      }

      uint32_t old_m_fwd_src = fwd->_metric_fwd_src;
      uint32_t old_m_to_fwd = fwd->_metric_to_fwd;

      fwd->update_entry(id, hops, metric_fwd_src, metric_to_fwd);

      // metric decreases and fwd the best
      if ( (((old_m_fwd_src+old_m_to_fwd) < (metric_fwd_src+metric_to_fwd)) &&
                memcmp(fwd_ea.data(), _best_forwarder.data(), 6 ) == 0 ) ||
              (old_m_fwd_src+old_m_to_fwd  == 0)) {
        update_best_fwd();                                                    //check wether fwd the best again
      } else { //metric increases (better metric)
        if (((old_m_fwd_src+old_m_to_fwd) > (metric_fwd_src+metric_to_fwd)) || (old_m_fwd_src+old_m_to_fwd  == 0) ) {
          if ( memcmp(fwd_ea.data(), _best_forwarder.data(), 6 ) == 0 ) {
            _best_metric = metric_fwd_src+metric_to_fwd;
            _hops_best_metric = hops;
          } else {
            if ( (metric_fwd_src+metric_to_fwd) < _best_metric ) {
              _best_forwarder = fwd_ea;
              _best_metric = metric_fwd_src+metric_to_fwd;
              _hops_best_metric = hops;
            }
          }
        }
      }
    }

    void update_best_fwd() {
      for (BatmanForwarderMapIter i = _forwarder.begin(); i.live(); i++) {
        BatmanForwarderEntry *bfe = _forwarder.findp(i.key());

        if ( ( bfe->_metric_fwd_src + bfe->_metric_to_fwd ) < _best_metric ) {
          _best_forwarder = bfe->_forwarder;
          _best_metric = bfe->_metric_fwd_src + bfe->_metric_to_fwd;
          _hops_best_metric = bfe->_hops;
        }
      }
    }

    void forward_node(uint32_t originator_id) {
      _last_forward_time = Timestamp::now();
      _last_forward_originator_id = originator_id;
    }

    unsigned int
    ones32(register unsigned int x)
    {
        /* 32-bit recursive reduction using SWAR...
      but first step is mapping 2-bit values
      into sum of 2 1-bit values in sneaky way
        */
      x -= ((x >> 1) & 0x55555555);
      x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
      x = (((x >> 4) + x) & 0x0f0f0f0f);
      x += (x >> 8);
      x += (x >> 16);
      return(x & 0x0000003f);
    }

    unsigned int
    floor_log2(register unsigned int x)
    {
      x |= (x >> 1);
      x |= (x >> 2);
      x |= (x >> 4);
      x |= (x >> 8);
      x |= (x >> 16);
#ifdef  LOG0UNDEFINED
      return(ones32(x) - 1);
#else
      return(ones32(x >> 1));
#endif
    }

    bool should_be_forwarded(uint32_t originator_id) {
      if (((_last_forward_originator_id % (floor_log2(_hops_best_metric)+1)) == originator_id % (floor_log2(_hops_best_metric)+1)) &&
            ( (Timestamp::now() - _latest_originator_time).msecval() < 60000 )) return true;

/*      click_chatter("the following info will not be forwarded: Node: %s Hops: %d LFWD: %d CIF: %d Time: %s",
                    _addr.unparse().c_str(), _hops_best_metric,
                    _last_forward_originator_id, originator_id,
                    _latest_originator_time.unparse().c_str());*/
      return false;
    }
  };

  typedef HashMap<EtherAddress,BatmanNode> BatmanNodeMap;
  typedef BatmanNodeMap::const_iterator BatmanNodeMapIter;

  typedef Vector<BatmanNode*> BatmanNodePList;

  //
  //methods
  //
  BatmanRoutingTable();
  ~BatmanRoutingTable();

  const char *class_name() const  { return "BatmanRoutingTable"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  BatmanRoutingTable::BatmanNode *addBatmanNode(EtherAddress addr);

 private:
  //
  //member
  //
  BatmanNodeMap _nodemap;

  BatmanNode *getBatmanNode(EtherAddress addr);

  Brn2LinkTable *_linktable;

  /*For Debug*/
  BRN2NodeIdentity *_nodeid;

 public:
  void update_originator(EtherAddress src, EtherAddress fwd, uint32_t id, int hops,
                          uint32_t metric_fwd_src, uint32_t metric_to_fwd);

  BatmanForwarderEntry *getBestForwarder(EtherAddress dst);

  void get_nodes_to_be_forwarded(int originator_id, BatmanNodePList *bnl);

  String print_rt();

  uint32_t get_link_metric(EtherAddress from, EtherAddress to) {
    return _linktable->get_link_metric( from, to);
  }

  uint32_t _originator_mode;
};

CLICK_ENDDECLS
#endif
