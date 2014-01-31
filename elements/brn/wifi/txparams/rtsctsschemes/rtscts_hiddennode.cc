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
  _cocst(NULL),
  _pessimistic(false)
{
}

void *
RtsCtsHiddenNode::cast(const char *name)
{
  if (strcmp(name, "RtsCtsHiddenNode") == 0)
    return (RtsCtsHiddenNode *) this;
  else if (strcmp(name, "RtsCtsScheme") == 0)
    return (RtsCtsScheme *) this;
  else
    return NULL;
}


RtsCtsHiddenNode::~RtsCtsHiddenNode()
{
}

int
RtsCtsHiddenNode::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "HIDDENNODE", cpkP+cpkM, cpElement, &_hnd,
    "COOPCHANNELSTATS", cpkP, cpElement, &_cocst,
    "PESSIMISTIC", cpkP, cpBool, &_pessimistic,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsHiddenNode::set_rtscts(EtherAddress &dst, uint32_t /*size*/)
{
  if ( _hnd->count_hidden_neighbours(dst) != 0 ) return true;
  if ( _pessimistic && (_hnd->count_hidden_neighbours(brn_etheraddress_broadcast) != 0) ) return true;

  if ( _cocst != NULL ) {
    
    
  }

  return false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsHiddenNode)

