#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "rtscts_hiddennode.hh"

CLICK_DECLS

RtsCtsHiddenNode::RtsCtsHiddenNode():
  _hnd(NULL),
  _pessimistic(false)
{
  _default_strategy = RTS_CTS_STRATEGY_HIDDENNODE;
}

void *
RtsCtsHiddenNode::cast(const char *name)
{
  if (strcmp(name, "RtsCtsHiddenNode") == 0)
    return reinterpret_cast<RtsCtsHiddenNode *>(this);

  return RtsCtsScheme::cast(name);
}


RtsCtsHiddenNode::~RtsCtsHiddenNode()
{
}

int
RtsCtsHiddenNode::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "HIDDENNODE", cpkP+cpkM, cpElement, &_hnd,
    "PESSIMISTIC", cpkP, cpBool, &_pessimistic,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsHiddenNode::set_rtscts(PacketInfo *pinfo)
{
  if ( _hnd->count_hidden_neighbours(pinfo->_dst) != 0 ) return true;
  if ( _pessimistic && (_hnd->count_hidden_neighbours(brn_etheraddress_broadcast) != 0) ) return true;

  return false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsHiddenNode)

