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
  _cocst(NULL)
{
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
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsHiddenNode::set_rtscts(EtherAddress &/*dst*/, uint32_t /*size*/)
{
  return true;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsHiddenNode)

