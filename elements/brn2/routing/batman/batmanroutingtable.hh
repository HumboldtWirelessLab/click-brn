#ifndef BATMANROUTINGTABLEELEMENT_HH
#define BATMANROUTINGTABLEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

CLICK_DECLS
/*
 * =c
 * BatmanRoutingTable()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNBroadcastRouting-Packets
 * Output 0 : BRNBroadcastRouting-Packets
 * Output 1 : Packets to local
  * =d
 */
#define MAX_QUEUE_SIZE  1500


class BatmanRoutingTable : public Element {

 public:

  class BatmanNeighbour
  {
    EtherAddress _addr;
  };

  typedef Vector<BatmanNeighbour> BatmanNeighbourList;
  typedef BatmanNeighbourList::const_iterator BatmanNeighbourListIter;

  class BatmanOriginatorInfo
  {
   public:
    uint32_t _originator_id;
    int _hops;

    BatmanOriginatorInfo( uint32_t originator_id, int hops )
    {
      _originator_id = originator_id;
      _hops = hops;
    }
  };

  typedef Vector<BatmanOriginatorInfo> BatmanOriginatorInfoList;
  typedef BatmanOriginatorInfoList::const_iterator BatmanOriginatorInfoIter;

  class BatmanForwarderEntry
  {
   public:
    EtherAddress _addr;
    BatmanOriginatorInfoList _oil;

    BatmanForwarderEntry(EtherAddress addr)
    {
      _addr = addr;
    }

    BatmanOriginatorInfoList *getOIL()
    {
      return &_oil;
    }
  };

  typedef Vector<BatmanForwarderEntry> BatmanForwarderList;
  typedef BatmanForwarderList::const_iterator BatmanForwarderListIter;

  class BatmanNode
  {
   public:
    EtherAddress _addr;
    uint32_t _originator_interval;
    uint32_t _last_originator_id;
    Timestamp _last_originator_time;

    BatmanForwarderList _forwarder;

    BatmanNode(EtherAddress addr)
    {
      _addr = addr;
      _originator_interval = 0;
      _last_originator_id = 0xffffffff;
      _last_originator_time = Timestamp::now();
    }

    ~BatmanNode()
    {
      _forwarder.clear();
    }

    BatmanForwarderEntry *getBatmanForwarderEntry(EtherAddress addr)
    {
      for ( int i = 0; i < _forwarder.size(); i++ )
      {
        if ( _forwarder[i]._addr == addr ) return &_forwarder[i];
      }

      return NULL;
    }

    int update_originator(uint32_t id)
    {
      if ( _last_originator_id > id ) {
        //TODO; clear lists ,....
        click_chatter("Restart originator");
        _last_originator_id = id;
        return 0;
      }

      _last_originator_id = id;
      //_last_originator_id
    }

    int add_originator( EtherAddress fwd, uint32_t id, int hops )
    {
      update_originator(id); //TODO: check to place it elsewhere

      BatmanForwarderEntry *bfe = getBatmanForwarderEntry(fwd);
      if ( bfe == NULL ) {
        _forwarder.push_back(BatmanForwarderEntry(fwd));
        bfe = getBatmanForwarderEntry(fwd);
      }

      BatmanOriginatorInfoList *oil = bfe->getOIL();

      int i;
      for ( int i = 0; i < oil->size(); i++ )
      {
        if ( oil->at(i)._originator_id == id ) {
          return -1;
        }
      }

      oil->push_back(BatmanOriginatorInfo(id,hops));
      return 0;
    }
  };

  typedef Vector<BatmanNode> BatmanNodeList;
  typedef BatmanNodeList::const_iterator BatmanNodeListIter;

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

 private:
  //
  //member
  //
  BatmanNeighbourList _neighbours;
  BatmanNodeList      _nodelist;

  BatmanNode *getBatmanNode(EtherAddress addr);
  int addBatmanNode(EtherAddress addr);

 public:
   void handleNewOriginator(uint32_t id, EtherAddress src, EtherAddress fwd, int hops);
   String infoGetNodes();
   int _debug;

};

CLICK_ENDDECLS
#endif
