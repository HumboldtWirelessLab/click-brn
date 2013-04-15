#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>

#include "analyzephantom.hh"
#include "elements/brn/wifi/brnwifi.hh"

CLICK_DECLS

AnalyzePhantom::AnalyzePhantom()
{
  BRNElement::init();
}


AnalyzePhantom::~AnalyzePhantom()
{
}


Packet *
AnalyzePhantom::simple_action(Packet *p)
{
  BRN_DEBUG("Analyze Phantom");

  return p;
}








CLICK_ENDDECLS


EXPORT_ELEMENT(AnalyzePhantom)
