#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"

#include "brn2_srcforwarder.hh"
#include "brn2_dsrhandlefeedback.hh"

CLICK_DECLS

DSRHandleFeedback::DSRHandleFeedback():
  _me()
{
  BRNElement::init();
}

DSRHandleFeedback::~DSRHandleFeedback()
{
}

int DSRHandleFeedback::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DSRHandleFeedback::initialize(ErrorHandler *)
{
  return 0;
}

void
DSRHandleFeedback::push( int port, Packet *packet )
{
  click_brn_dsr *brn_dsr = (click_brn_dsr *)(packet->data() + sizeof(click_brn));

  if ( brn_dsr->dsr_type != BRN_DSR_SRC ) {
    if ( port == 1 ) output(0).push(packet);
    else packet->kill();
    return;
  }

  EtherAddress src(brn_dsr->dsr_src.data);
  EtherAddress dst(brn_dsr->dsr_dst.data);

  BRN_DEBUG("Found DSR-Header: Src %s Dst: %s",src.unparse().c_str(),dst.unparse().c_str());

  if ( _me->isIdentical(&src) ) { 

    if ( port == 1 ) output(0).push(packet->clone()->uniqueify()); //packet for dsr (route error)
    
    packet = BRN2SrcForwarder::strip_all_headers(packet);

    output(1).push(packet);
  
  } else {
    if ( port == 1 ) { //errors are fwd to dsr (route error)
      output(0).push(packet);
    } else {
      packet->kill();
    }
  }
}

enum {H_STATS};

String
DSRHandleFeedback::read_stats()
{
  StringAccum sa;

  sa << "<dsr_handle_feedback id=\"" << BRN_NODE_NAME << "\" >\n";

  sa << "</dsr_handle_feedback>\n";

  return sa.take_string();
}

static String 
DSRHandleFeedback_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  DSRHandleFeedback *td = (DSRHandleFeedback *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->read_stats();
      break;
  }

  return String();
}

void
DSRHandleFeedback::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", DSRHandleFeedback_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DSRHandleFeedback)
