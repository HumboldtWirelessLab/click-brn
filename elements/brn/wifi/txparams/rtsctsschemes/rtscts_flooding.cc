#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "rtscts_flooding.hh"

CLICK_DECLS

RtsCtsFlooding::RtsCtsFlooding():
  _flooding(NULL),
  _fhelper(NULL),
  _hnd(NULL),
  _pessimistic(false)
{
  _default_strategy = RTS_CTS_STRATEGY_FLOODING;
}

void *
RtsCtsFlooding::cast(const char *name)
{
  if (strcmp(name, "RtsCtsFlooding") == 0)
    return (RtsCtsFlooding *) this;

  return RtsCtsScheme::cast(name);
}


RtsCtsFlooding::~RtsCtsFlooding()
{
}

int
RtsCtsFlooding::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "FLOODING", cpkP+cpkM, cpElement, &_flooding,
    "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
    "HIDDENNODE", cpkP+cpkM, cpElement, &_hnd,
    "PESSIMISTIC", cpkP, cpBool, &_pessimistic,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsFlooding::set_rtscts(PacketInfo *pinfo)
{
  Vector<EtherAddress> hiddennodes;

  if (_pessimistic) _hnd->get_hidden_neighbours(brn_etheraddress_broadcast, &hiddennodes);
  else              _hnd->get_hidden_neighbours(pinfo->_dst, &hiddennodes);

  uint16_t bcast_id;
  EtherAddress *bcast_src = _flooding->get_last_tx(&bcast_id);

  //get all known nodes
  struct Flooding::BroadcastNode::flooding_last_node *last_nodes;
  uint32_t last_nodes_size;
  last_nodes = _flooding->get_last_nodes(bcast_src, bcast_id, &last_nodes_size);

  for (Vector<EtherAddress>::const_iterator ea_iter = hiddennodes.begin(); ea_iter != hiddennodes.end(); ea_iter++) {
    BRN_DEBUG("HN: %s", ea_iter->unparse().c_str());
  }

  for (uint32_t j = 0; j < last_nodes_size; j++ ) {
    BRN_DEBUG("LN: %s", EtherAddress(last_nodes[j].etheraddr).unparse().c_str());
  }

  uint16_t eff_hn = hiddennodes.size();

  for (Vector<EtherAddress>::iterator ea_iter = hiddennodes.begin(); ea_iter != hiddennodes.end(); ea_iter++) {
    uint32_t j = 0;
    for (; j < last_nodes_size; j++ ) {
      if ((last_nodes[j].flags & FLOODING_LAST_NODE_FLAGS_RX_ACKED) == 0) {
        BRN_DEBUG("comp: %s %s %d", EtherAddress(last_nodes[j].etheraddr).unparse().c_str(), ea_iter->unparse().c_str(), eff_hn);
        if (EtherAddress(last_nodes[j].etheraddr) == *ea_iter) break;
      }
    }
    if (j == last_nodes_size) eff_hn--;
  }

  BRN_DEBUG("EFF HN: %d",eff_hn);

  return eff_hn > 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsFlooding)

