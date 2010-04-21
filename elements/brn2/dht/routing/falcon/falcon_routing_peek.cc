#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/standard/md5.h"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_routing_peek.hh"

CLICK_DECLS

FalconRoutingPeek::FalconRoutingPeek()
{
  BRNElement::init();
}

FalconRoutingPeek::~FalconRoutingPeek()
{
}

int FalconRoutingPeek::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "ROUTINGPEEK", cpkP+cpkM, cpElement, &_routing_peek,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

static bool routing_peek_func(void *e, Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port)
{
  return ((FalconRoutingPeek *)e)->handle_routing_peek(p, src, dst, brn_port);
}

int FalconRoutingPeek::initialize(ErrorHandler *)
{
  _routing_peek->add_routing_peek(routing_peek_func, (void*)this, BRN_PORT_DHTROUTING );
  return 0;
}

bool
FalconRoutingPeek::handle_routing_peek(Packet *p, EtherAddress *src, EtherAddress *dst, int brn_port)
{
  if ( ( p != NULL ) && ( brn_port == BRN_PORT_DHTROUTING ) && ( *src != _frt->_me->_ether_addr ) ) {
    BRN_DEBUG("Handle Routing Peek. Src: %s Dst: %s",src->unparse().c_str(), dst->unparse().c_str());

    BRNProtocol::pull_brn_header(p);

    switch ( DHTProtocol::get_type(p) ) {
      case FALCON_MINOR_REQUEST_SUCCESSOR:
      case FALCON_MINOR_REQUEST_POSITION:
        handle_request(p);
        break;
      case FALCON_MINOR_REPLY_SUCCESSOR:
      case FALCON_MINOR_UPDATE_SUCCESSOR:
      case FALCON_MINOR_REPLY_POSITION:
        handle_reply(p);
        break;
      case FALCON_MINOR_LEAVE_NETWORK_NOTIFICATION:
      case FALCON_MINOR_LEAVE_NETWORK_REPLY:
        handle_leave(p);
        break;
      case FALCON_MINOR_NWS_REQUEST:
        handle_nws(p);
        break;
      default:
        break;
    }

    BRNProtocol::push_brn_header(p);
  }
  return true;
}

void
FalconRoutingPeek::handle_request(Packet *p)
{
  uint16_t position;
  DHTnode dst, src;

  BRN_DEBUG("handle request");

  DHTProtocolFalcon::get_info(p, &src, &dst, &position);
  _frt->add_node(&src);
}

void
FalconRoutingPeek::handle_reply(Packet *p)
{
  uint16_t position;
  DHTnode dst, src;

  BRN_DEBUG("handle reply");

  DHTProtocolFalcon::get_info(p, &src, &dst, &position);
  _frt->add_node(&src);
  _frt->add_node(&dst);
}

void
FalconRoutingPeek::handle_leave(Packet */*p*/)
{

}

void
FalconRoutingPeek::handle_nws(Packet */*p*/)
{

}

void
FalconRoutingPeek::add_handlers()
{
  BRNElement::add_handlers();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingPeek)
