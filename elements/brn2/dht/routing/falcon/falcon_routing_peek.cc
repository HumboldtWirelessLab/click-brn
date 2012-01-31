#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brnprotocol/brnprotocol.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_functions.hh"
#include "falcon_routingtable.hh"
#include "falcon_routing_peek.hh"

CLICK_DECLS

FalconRoutingPeek::FalconRoutingPeek():
  _reroute_req(false),
  _active(false)
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
      "REROUTEREQUESTS", cpkN, cpBool, &_reroute_req,
      "ACTIVE", cpkN, cpBool, &_active,
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
  bool cont_forwarding = true;

  if ( ( p != NULL ) && ( brn_port == BRN_PORT_DHTROUTING ) && ( *src != _frt->_me->_ether_addr ) && ( *dst != _frt->_me->_ether_addr ) ) {
    BRN_DEBUG("Handle Routing Peek. Src: %s Dst: %s",src->unparse().c_str(), dst->unparse().c_str());

    BRNProtocol::pull_brn_header(p);

    switch ( DHTProtocol::get_type(p) ) {
      case FALCON_MINOR_REQUEST_SUCCESSOR:
      case FALCON_MINOR_REQUEST_POSITION:
        cont_forwarding = handle_request(p);
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

    if (cont_forwarding) BRNProtocol::push_brn_header(p);
  }
  return cont_forwarding;
}

bool
FalconRoutingPeek::handle_request(Packet *p)
{
  uint16_t position;
  DHTnode dst, src;

  BRN_DEBUG("handle request");

  DHTProtocolFalcon::get_info(p, &src, &dst, &position);
  _frt->add_node(&src);
  _frt->add_node(&dst);

  if ( DHTProtocol::get_type(p) == FALCON_MINOR_REQUEST_SUCCESSOR ) {
    DHTnode *best_succ = _frt->findBestSuccessor(&src, 20,NULL);

    if ( ! best_succ->equals(&dst) && ( FalconFunctions::is_in_between(&src, &dst, best_succ))) { //"is_in_between" makes sure that the found succ is better then current dst
      BRN_WARN("---------------------------------------------------- Found better Successor in PEEK");
      char srcp[128];
      char o_succ[128];
      char suss[128];
      MD5::printDigest(src._md5_digest,srcp);
      MD5::printDigest(dst._md5_digest,o_succ);
      MD5::printDigest(best_succ->_md5_digest,suss);
      BRN_WARN("Src: %s OLD_SUCC: %s Best_SUCC: %s",srcp, o_succ, suss);
      BRN_WARN("Src: %s OLD_SUCC: %s Best_SUCC: %s",src._ether_addr.unparse().c_str(), dst._ether_addr.unparse().c_str(), best_succ->_ether_addr.unparse().c_str());

      if ( ! _reroute_req ) return true;

      BRN_DEBUG("Start Reroute");

      WritablePacket *fwd_p;

      if ( best_succ->equals(_frt->_me) ) {  //if me send reply
        fwd_p = DHTProtocolFalcon::new_route_reply_packet(_frt->_me, &src, FALCON_MINOR_REPLY_SUCCESSOR,
                                                          _frt->_me, FALCON_RT_POSITION_SUCCESSOR);
        p->kill();
      } else
        fwd_p = DHTProtocolFalcon::fwd_route_request_packet(&src, best_succ, _frt->_me, p);

      output(0).push(fwd_p);

      return false;
    }
  }

  return true;
}

void
FalconRoutingPeek::handle_reply(Packet *p)
{
  uint16_t position;
  DHTnode dst, src;

  BRN_DEBUG("handle reply");

  if ( ! _active ) return;

  DHTProtocolFalcon::get_info(p, &src, &dst, &position);
  _frt->add_node(&src);
  _frt->add_node(&dst);

  if ( position == FALCON_RT_POSITION_SUCCESSOR ) {
    DHTnode *best_succ = _frt->findBestSuccessor(&src, 20,NULL);
    if ( ! best_succ->equals(&dst) ) {
      BRN_WARN("I found a better successor. Change reply.");
    }
  }
}

void
FalconRoutingPeek::handle_leave(Packet */*p*/)
{
  if ( ! _active ) return;
}

void
FalconRoutingPeek::handle_nws(Packet */*p*/)
{
  if ( ! _active ) return;
}

void
FalconRoutingPeek::add_handlers()
{
  BRNElement::add_handlers();
}


CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingPeek)
