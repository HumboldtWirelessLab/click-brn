#ifndef DCLUSTERELEMENT_HH
#define DCLUSTERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"

#include "dclusterprotocol.hh"

/**
 NOTE: Paper: Max-Min D-Clusetr Formation in Wireless Ad Hoc Networks

 TODO:
*/

CLICK_DECLS

class DCluster : public Element {

 public:

  class ClusterNodeInfo {
   public:
    uint32_t _id;
    uint32_t _distance;

    EtherAddress _ether_addr;

    EtherAddress _info_src;

    ClusterNodeInfo(const EtherAddress *ea, uint32_t id, uint32_t distance) {
      _ether_addr = EtherAddress(ea->data());
      _id = id;
      _distance = distance;
    }

    ClusterNodeInfo() {
      _id = 0;
      _distance = DCLUSTER_INVALID_ROUND;
    }

    void setInfo(uint8_t *addr, uint32_t id, uint32_t distance) {
      _ether_addr = EtherAddress(addr);
      _id = id;
      _distance = distance;
    }

    void setInfo(EtherAddress *ea, uint32_t id, uint32_t distance) {
      setInfo(ea->data(), id, distance);
    }

    void setInfo(ClusterNodeInfo info) {
      setInfo(info._ether_addr.data(), info._id, info._distance);
    }

    void storeStruct(struct dcluster_node_info *node_info) {
      node_info->id = htonl(_id);
      memcpy(node_info->etheraddr, _ether_addr.data(), 6);
      node_info->hops = _distance;
    }

  };


 public:
  //
  //methods
  //
  DCluster();
  ~DCluster();

  const char *class_name() const  { return "DCluster"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(char *buffer, int size);

  String get_info();
  DCluster::ClusterNodeInfo* selectClusterHead();
  DCluster::ClusterNodeInfo* getClusterHead() { return _cluster_head;}
  void informClusterHead(DCluster::ClusterNodeInfo*);
 private:
  //
  //member
  //

  BRN2LinkStat *_linkstat;
  BRN2NodeIdentity *_node_identity;

 public:

  int _debug;

  int _max_distance;

  int _my_min_round;
  int _my_max_round;

  int _ac_min_round;
  int _ac_max_round;

  ClusterNodeInfo _my_info;
  ClusterNodeInfo *_cluster_head;

  ClusterNodeInfo *_max_round;
  ClusterNodeInfo *_min_round;

  int _delay;
};

CLICK_ENDDECLS
#endif
