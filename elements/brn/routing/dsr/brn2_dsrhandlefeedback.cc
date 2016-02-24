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
  _me(),
  _link_table(NULL)
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
      "LINKTABLE", cpkP, cpElement, &_link_table,
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
  const click_brn_dsr *brn_dsr = reinterpret_cast<const click_brn_dsr *>((packet->data() + sizeof(click_brn)));

  if ( brn_dsr->dsr_type != BRN_DSR_SRC ) {
    if ( port == 1 ) output(0).push(packet);
    else packet->kill();
    return;
  }

  EtherAddress ether_src(BRNPacketAnno::src_ether_anno(packet));
  EtherAddress ether_dst(BRNPacketAnno::dst_ether_anno(packet));

  EtherAddress src(brn_dsr->dsr_src.data);
  EtherAddress dst(brn_dsr->dsr_dst.data);

  int metric = (_link_table)?_link_table->get_link_metric(ether_src, ether_dst):9999;

  if ( port == 1 ) { //Failed
    BRN_DEBUG("TXFailed: Found DSR-Header: Route : %s -> %s Link: %s -> %s (%d)",src.unparse().c_str(),dst.unparse().c_str(),ether_src.unparse().c_str(),ether_dst.unparse().c_str(),metric);
  } else {
    BRN_DEBUG("TXSuccess: Found DSR-Header: Route : %s -> %s Link: %s -> %s (%d)",src.unparse().c_str(),dst.unparse().c_str(),ether_src.unparse().c_str(),ether_dst.unparse().c_str(),metric);
  }

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
  DSRHandleFeedback *td = reinterpret_cast<DSRHandleFeedback *>(e);
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
