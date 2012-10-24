#ifndef EEWSSTATEELEMENT_HH
#define EEWSSTATEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinktable.hh"
#include "elements/brn/topology/nhopneighbouring/nhopneighbouring_info.hh"

#include "elements/brn/services/sensor/gps/gps.hh"

#define UPDATE_ALARM_NEW_NODE       1
#define UPDATE_ALARM_NEW_ID         2
#define UPDATE_ALARM_UPDATE_HOPS    4
#define UPDATE_ALARM_NEW_FORWARDER  8

#define UPDATE_ALARM_NEED_FORWARD ( UPDATE_ALARM_NEW_NODE | UPDATE_ALARM_NEW_ID | UPDATE_ALARM_UPDATE_HOPS )

#define DEFAULT_MIN_NEIGHBOUR_FRACTION 100

#define STATE_NO_TRIGGER_NO_ALARM			0	/* idle */
#define STATE_YES_TRIGGER_NO_ALARM			1	/* false alarm or waiting for other nodes */
#define STATE_YES_TRIGGER_OWN_ALARM			2	/* P-Wave arrived, and also other triggers known */
#define STATE_NO_TRIGGER_OWN_ALARM			3	/* best because fastest, alarm even before P-Wave arrived */
#define STATE_YES_TRIGGER_NEIGHBOR_ALARM	4	/* P-Wave arrived, but no own alarm but informed by neighbor */
#define STATE_NO_TRIGGER_NEIGHBOR_ALARM		5	/* no own trigger (yet) and no alarm, hence alarm only by neighbor */

#define STATE_DEFAULT 0 /* nothing, idle */
#define STATE_HAVE_TRIGGER    1
#define STATE_HAVE_ALARM      2
#define STATE_HAVE_NEIGBOR_ALARM 4



//#define STATE_NO_TRIGGER_NO_ALARM       0 /* idle */
//#define STATE_YES_TRIGGER_NO_ALARM      1 /* false alarm or waiting for other nodes */
//#define STATE_YES_TRIGGER_OWN_ALARM     2 /* P-Wave arrived, and also other triggers known */
//#define STATE_NO_TRIGGER_OWN_ALARM      3 /* best because fastest, alarm even before P-Wave arrived */
//#define STATE_YES_TRIGGER_NEIGHBOR_ALARM  4 /* P-Wave arrived, but no own alarm but informed by neighbor */
//#define STATE_NO_TRIGGER_NEIGHBOR_ALARM   5 /* no own trigger (yet) and no alarm, hence alarm only by neighbor */



CLICK_DECLS

class EewsState : public BRNElement {

 public:

  class ForwarderInfo {
   public:
    EtherAddress _ea;

    ForwarderInfo(const EtherAddress *ea) {
      _ea= *ea;
    }
  };

  class AlarmInfo {
   public:
    uint8_t _id;
    uint8_t _hops;

    /*TODO: use vector of Etheraddress. no need for FwdInfo*/
    Vector<ForwarderInfo> _fwd;
    bool _fwd_missing;
    int _fract_fwd;
    int _retries;

    AlarmInfo(uint8_t id, uint8_t hops) {
      _id = id;
      _hops = hops;
      _fwd_missing = false;
      _fract_fwd = 0;
      _retries = 0;
    }

    ForwarderInfo *get_fwd_by_address(const EtherAddress *ea) {
      for ( int i = 0; i < _fwd.size(); i++ )
        if ( _fwd[i]._ea == *ea ) return &_fwd[i];

      return NULL;
    }

    void add_forwarder(const EtherAddress *ea) {
      _fwd.push_back(ForwarderInfo(ea));
    }
  };

  class AlarmNode {
   public:
    EtherAddress _ea;
    uint8_t _type;

    AlarmNode( uint8_t type, const EtherAddress *ea) {
      _type = type;
      _ea = *ea;
    }

    Vector<AlarmInfo> _info;

    AlarmInfo *get_info_by_id(uint8_t id) {
      for ( int i = 0; i < _info.size(); i++ )
        if ( _info[i]._id == id ) return &_info[i];

      return NULL;
    }

    void add_alarm_info( uint8_t id, uint8_t hops) {
      _info.push_back(AlarmInfo(id, hops));
    }
  };

 public:
  //
  //methods
  //
  EewsState();
  ~EewsState();

  const char *class_name() const  { return "EewsState"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();



  Vector<AlarmNode> _alarm_nodes;

  AlarmNode *get_node_by_address(uint8_t type, const EtherAddress *ea);

  uint32_t update_alarm(int type, const EtherAddress *src, int id, int hops, const EtherAddress *fwd);
  void update_neighbours();

  void get_incomlete_forward_types(int max_fraction, Vector<int> *types);
  void get_incomlete_forward_nodes(int max_fraction, int max_retries, int max_hops, int type, Vector<AlarmNode*> *nodes);

  void trigger_alarm();
  String get_state();

  uint8_t getState() { return _triggered_alarm; }

  GPSPosition *getPosition() { return _gps->getPosition();}

  void check_neighbor_alarm(uint8_t state, const EtherAddress *ea);
  void update_me(uint8_t state);


  int _hop_limit;
  uint32_t _forward_flags;

 private:

  Brn2LinkTable *_lt;
  NHopNeighbouringInfo *_nhopn_info;
  GPS *_gps;

 public:
  int _retry_limit;

  int _min_neighbour_fraction;
  uint32_t _min_alarm_fraction;

  uint8_t _triggered_alarm;
  Timestamp _triggered_alarm_time;
  GPSPosition _position;

};

CLICK_ENDDECLS
#endif
