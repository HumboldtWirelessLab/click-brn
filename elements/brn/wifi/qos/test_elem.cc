#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "test_elem.hh"

CLICK_DECLS

TestElem::TestElem() { BRNElement::init(); }


int
TestElem::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "CWMIN", cpkP, cpString, &s_cwmin,
      "CWMAX", cpkP, cpString, &s_cwmax,
      "AIFS", cpkP, cpString, &s_aifs,
      "STRATEGY", cpkP, cpInteger, &_bqs_strategy,
      "CHANNELSTATS", cpkP, cpElement, &_cst,
      "COLLISIONINFO", cpkP, cpElement, &_colinf,
      "TARGETPER", cpkP, cpInteger, &_target_packetloss,
      "TARGETCHANNELLOAD", cpkP, cpInteger, &_target_channelload,
      "TARGETRXTXBUSYDIFF", cpkP, cpInteger, &_target_diff_rxtx_busy,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

void TestElem add_handlers() {}

Packet *TestElem::simple_action(Packet *p) { return p; }

void TestElem::test() { click_chatter("TestElem!"); }

CLICK_ENDDECLS
EXPORT_ELEMENT(TestElem)
ELEMENT_MT_SAFE(TestElem)
