#ifndef DCLUSTERELEMENT_HH
#define DCLUSTERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn/cluster/clustering.hh"
#include "dclusterprotocol.hh"

/**
 NOTE: Paper: Max-Min D-Cluster Formation in Wireless Ad Hoc Networks

 TODO:
*/

CLICK_DECLS

class DCluster : public Clustering {

 public:

  class ClusterNodeInfo: public Clustering::Cluster {
   public:
    uint32_t _distance;

    EtherAddress _ether_addr;

    EtherAddress _info_src;

    ClusterNodeInfo(const EtherAddress *ea, uint32_t id, uint32_t distance): Cluster() {
      _ether_addr = EtherAddress(ea->data());
      _cluster_id = id;
      _distance = distance;
    }

    ClusterNodeInfo() {
      _cluster_id = 0;
      _distance = DCLUSTER_INVALID_ROUND;
    }

    void setInfo(uint8_t *addr, uint32_t id, uint32_t distance) {
      _ether_addr = EtherAddress(addr);
      _cluster_id = id;
      _distance = distance;
    }

    void setInfo(EtherAddress *ea, uint32_t id, uint32_t distance) {
      setInfo(ea->data(), id, distance);
    }

    void setInfo(ClusterNodeInfo info) {
      setInfo(info._ether_addr.data(), info._cluster_id, info._distance);
    }

    void storeStruct(struct dcluster_node_info *node_info) {
      node_info->id = htonl(_cluster_id);
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
  const char *clustering_name() const { return "DCluster"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  int lpSendHandler(char *buffer, int size);
  int lpReceiveHandler(char *buffer, int size);
  void clustering_process();

  bool clusterhead_is_me() { return _my_info._ether_addr == _cluster_head->_ether_addr; }
  String get_info();
  DCluster::ClusterNodeInfo* selectClusterHead();
  DCluster::ClusterNodeInfo* getClusterHead() { return _cluster_head;}
  void informClusterHead(DCluster::ClusterNodeInfo*);

  void init_cluster_node_info();

 public:

  int _max_distance; //TODO: check usage

  int _max_no_min_rounds;
  int _max_no_max_rounds;

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
