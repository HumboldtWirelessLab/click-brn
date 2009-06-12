#ifndef BATMANROUTINGTABLEELEMENT_HH
#define BATMANROUTINGTABLEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "elements/brn2/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

class BatmanRoutingTable : public Element {

 public:

  class BatmanNeighbour
  {
   public:
    EtherAddress _addr;
    int recvDeviceId;
    Timestamp _last_originator_time;

    BatmanNeighbour(EtherAddress addr) {
      _addr = addr;
      _last_originator_time = Timestamp::now();
    }
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
    int _recvDeviceId;
    BatmanOriginatorInfoList _oil;

    BatmanForwarderEntry(EtherAddress addr, uint8_t devId)
    {
      _addr = addr;
      _recvDeviceId = devId;
    }

    BatmanOriginatorInfoList *getOIL()
    {
      return &_oil;
    }

    int getAverageHopCount()
    {
      int hopsSum = 0;

      for ( int i =  0; i < _oil.size(); i++ )
        hopsSum += _oil[i]._hops;

      return hopsSum/_oil.size();
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

    BatmanForwarderEntry *getBatmanForwarderEntry(EtherAddress addr, uint8_t devId)
    {
      for ( int i = 0; i < _forwarder.size(); i++ )
      {
        if ( ( _forwarder[i]._addr == addr ) && ( _forwarder[i]._recvDeviceId == devId ) )
          return &_forwarder[i];
      }

      return NULL;
    }

    void update_originator(uint32_t id)
    {
      if ( _last_originator_id > id ) {
        //TODO; clear lists ,....
        //click_chatter("Restart originator: %d > %d",_last_originator_id , id);
        _last_originator_id = id;

        return;
      }

      _last_originator_id = id;
      //_last_originator_id
    }

    int add_originator( EtherAddress fwd, uint8_t devId, uint32_t id, int hops )
    {
      update_originator(id); //TODO: check to place it elsewhere

      BatmanForwarderEntry *bfe = getBatmanForwarderEntry(fwd, devId);
      if ( bfe == NULL ) {
        _forwarder.push_back(BatmanForwarderEntry(fwd, devId));
        bfe = getBatmanForwarderEntry(fwd, devId);
      }

      BatmanOriginatorInfoList *oil = bfe->getOIL();

      for ( int i = 0; i < oil->size(); i++ )
      {
        if ( oil->at(i)._originator_id == id ) {
          return -1;
        }
      }

      oil->push_back(BatmanOriginatorInfo(id,hops));
      return 0;
    }

    //TODO: better check
    bool gotOriginator(uint32_t id) {
      if ( _last_originator_id == 0xffffffff ) return false;
      return ( id == _last_originator_id );
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

  /*For Debug*/
  BRN2NodeIdentity *_nodeid;

 public:
   bool isNewOriginator(uint32_t id, EtherAddress src);
   void handleNewOriginator(uint32_t id, EtherAddress src, EtherAddress fwd, uint8_t devId, int hops);
   void addBatmanNeighbour(EtherAddress addr);

   BatmanForwarderEntry *getBestForwarder(EtherAddress dst);

   String infoGetNodes();
   int _debug;

};

CLICK_ENDDECLS
#endif
