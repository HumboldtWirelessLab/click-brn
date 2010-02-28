#ifndef NHOPCLUSTERELEMENT_HH
#define NHOPCLUSTERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

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

class NHopCluster : public Element {

 public:

  class ClusterHead {
   public:
    uint32_t _distance;

    EtherAddress _ether_addr;

    ClusterHead() {
    }

    ClusterHead(const EtherAddress *ea, uint32_t distance) {
      _ether_addr = EtherAddress(ea->data());
      _distance = distance;
    }

    void setInfo(uint8_t *addr, uint32_t distance) {
      _ether_addr = EtherAddress(addr);
      _distance = distance;
    }

    void setInfo(EtherAddress *ea, uint32_t distance) {
      setInfo(ea->data(), distance);
    }

    void setInfo(struct nhopcluster_lp_info *lpi) {
      _distance = lpi->hops;
      _ether_addr = EtherAddress(lpi->clusterhead);
    }

    void setInfo(struct nhopcluster_managment *nhcm) {
      _distance = nhcm->hops;
      _ether_addr = EtherAddress(nhcm->clusterhead);
    }

    void getInfo(struct nhopcluster_lp_info *lpi) {
      lpi->hops = _distance;
      memcpy(lpi->clusterhead, _ether_addr.data(), 6);
    }

  };


 public:
  //
  //methods
  //
  NHopCluster();
  ~NHopCluster();

  const char *class_name() const  { return "NHopCluster"; }
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

  void push(int, Packet *);

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(char *buffer, int size);

  static void static_nhop_timer_hook(Timer *t, void *f);
  void timer_hook();

  String get_info();

 private:
  //
  //member
  //

  BRN2LinkStat *_linkstat;
  BRN2NodeIdentity *_node_identity;

  Timer _nhop_timer;
  int _mode;


  void send_request();
  void send_notify();


  void handle_request(Packet *p);
  void handle_reply(Packet *p);
  void handle_notification(Packet *p);
  void forward(Packet *p);

 public:

  int _debug;

  int _max_distance;

  ClusterHead _cluster_head;
  bool _cluster_head_selected;

  int _delay;

  int _start,_startdelay;

  int _send_notification, _fwd_notification, _send_req, _fwd_req;
};

CLICK_ENDDECLS
#endif
