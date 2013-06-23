#ifndef NHOPCLUSTERELEMENT_HH
#define NHOPCLUSTERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"

#include "elements/brn/cluster/clustering.hh"
#include "nhopclusterprotocol.hh"

/**
 NOTE:

 TODO:
*/

CLICK_DECLS


#define NHOP_MODE_START      0
#define NHOP_MODE_REQUEST    1
#define NHOP_MODE_NOTIFY     2
#define NHOP_MODE_CONFIGURED 3

#define NHOP_DEFAULTSTART      5000
#define NHOP_DEFAULTSTARTDELAY 5000

class NHopCluster : public Clustering {

 public:

  class ClusterHead: public Cluster {
   public:
    uint32_t _distance;

    EtherAddress _best_next_hop; //TODO: List of next hops

    ClusterHead() {
    }

    ClusterHead(const EtherAddress *ea, uint32_t id, uint32_t distance): Cluster(*ea, id) {
      _distance = distance;
    }

    void setInfo(uint8_t *addr, uint32_t id, uint32_t distance) {
      Cluster::setInfo( EtherAddress(addr), id);
      _distance = distance;
    }

    void setInfo(EtherAddress *ea, uint32_t id, uint32_t distance) {
      setInfo(ea->data(), id, distance);
    }

    void setInfo(struct nhopcluster_lp_info *lpi) {
      Cluster::setInfo( EtherAddress(lpi->clusterhead), ntohl(lpi->id));
      _distance = lpi->hops;
    }

    void setInfo(struct nhopcluster_managment *nhcm) {
      Cluster::setInfo( EtherAddress(nhcm->clusterhead), ntohl(nhcm->id));
      _distance = nhcm->hops;
    }

    void getInfo(struct nhopcluster_lp_info *lpi) {
      lpi->hops = _distance;
      lpi->id = htonl(_cluster_id);
      memcpy( lpi->clusterhead, _clusterhead.data(), 6);

    }

    void setBestNextHop(EtherAddress *nh) {
      _best_next_hop = EtherAddress(nh->data());
    }
  };

 public:
  //
  //methods
  //
  NHopCluster();
  ~NHopCluster();

  const char *class_name() const  { return "NHopCluster"; }
  const char *clustering_name() const { return "NHopCluster"; }
  const char *processing() const  { return AGNOSTIC; }

  /**
   *  Input 0:  Everything
   *  Output 0: Non-Broadcast
   *  Output 1: Broadcast
  **/
  const char *port_count() const  { return "1/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  bool clusterhead_is_me() { return *(_node_identity->getMasterAddress()) == _cluster_head->_clusterhead; }

  void push(int, Packet *);

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(EtherAddress *src, char *buffer, int size);

  static void static_nhop_timer_hook(Timer *t, void *f);
  void timer_hook();

  String get_info();

 private:
  Timer _nhop_timer;
  int _mode;


  void send_request();
  void send_notify();


  void handle_request(Packet *p);
  void handle_reply(Packet *p);
  void handle_notification(Packet *p);
  void forward(Packet *p);

 public:

  int _max_distance;

  ClusterHead* _cluster_head;
  bool _cluster_head_selected;

  int _delay;

  int _start, _startdelay;

  int _send_notification, _fwd_notification, _send_req, _fwd_req;
};

CLICK_ENDDECLS
#endif
