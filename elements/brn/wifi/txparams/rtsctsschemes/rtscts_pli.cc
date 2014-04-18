#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "rtscts_pli.hh"

CLICK_DECLS

RtsCtsPLI::RtsCtsPLI()
{
  _default_strategy = RTS_CTS_STRATEGY_PLI;
}

void *
RtsCtsPLI::cast(const char *name)
{
  if (strcmp(name, "RtsCtsPLI") == 0)
    return (RtsCtsPLI *) this;

  return RtsCtsScheme::cast(name);
}

RtsCtsPLI::~RtsCtsPLI()
{
}

int
RtsCtsPLI::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
    "PLI", cpkP, cpElement, &_pli,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsPLI::set_rtscts(PacketInfo *pinfo)
{
  bool setrtscts = false;

  BRN_DEBUG("In Dest-Test: Destination: %s; ", pinfo->_dst.unparse().c_str());//, src.unparse().c_str());
  if(NULL != _pli) {

    BRN_DEBUG("Before pli_graph");
    PacketLossInformation_Graph *pli_graph = _pli->graph_get(pinfo->_dst);
    BRN_DEBUG("AFTER pli_graph");

    if (NULL == pli_graph) {
      BRN_DEBUG("There is not a Graph available for the DST-Adress: %s", pinfo->_dst.unparse().c_str());
      _pli->graph_insert(pinfo->_dst);
      BRN_DEBUG("Destination-Adress was inserted ");
      pli_graph = _pli->graph_get(pinfo->_dst);
    }

    BRN_DEBUG("There is a Graph available for the DST-Adress: %s", pinfo->_dst.unparse().c_str());
    PacketLossReason *pli_reason =  pli_graph->reason_get("hidden_node");
    unsigned int frac = pli_reason->getFraction();
    BRN_DEBUG("HIDDEN-NODE-FRACTION := %d", frac);

    // decide if RTS/CTS is on or off
    setrtscts = (click_random(0, 100) > frac)?true:false;
  }

  return setrtscts;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsPLI)

