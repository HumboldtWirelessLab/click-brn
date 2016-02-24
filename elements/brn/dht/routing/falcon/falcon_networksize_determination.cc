#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_functions.hh"
#include "falcon_routingtable.hh"
#include "falcon_networksize_determination.hh"

CLICK_DECLS

FalconNetworkSizeDetermination::FalconNetworkSizeDetermination():
  _frt(NULL),
  _networksize(1)
{
  BRNElement::init();
}

FalconNetworkSizeDetermination::~FalconNetworkSizeDetermination()
{
}

int FalconNetworkSizeDetermination::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconNetworkSizeDetermination::initialize(ErrorHandler *)
{
  return 0;
}

void
FalconNetworkSizeDetermination::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) )
    handle_nws(packet);
}

DHTnode*
FalconNetworkSizeDetermination::get_responsibly_node_FT(md5_byte_t *key, uint8_t *position)
{
  DHTnode *best = NULL;

  //TODO: check, whether this if-part can be part of the for-part (successor is part of FT (position 0))
  if ( FalconFunctions::is_equals( _frt->successor, key ) ) {
    *position = 0;
    return _frt->successor;
  }

  for ( int i = ( _frt->_fingertable.size() - 1 ); i > 0; i-- ) {
    if ( ! FalconFunctions::is_in_between( _frt->successor, _frt->_fingertable.get_dhtnode(i), key) ) {
      best = _frt->_fingertable.get_dhtnode(i);
      *position = i;
      break;
    }
  }

  return best;
}

void
FalconNetworkSizeDetermination::request_nws()
{
  uint8_t position;
  uint32_t size;

  DHTnode *next;

  BRN_DEBUG("New NWS-Request");

  if ( _frt->successor == NULL ) _networksize = 1;
  else if ( _frt->successor->equals(_frt->predecessor) ) {
    _networksize = 2;
  } else {
    next = get_responsibly_node_FT( _frt->_me->_md5_digest, &position);

    if (next == NULL) {
      BRN_ERROR("Next is NULL.");
      _networksize = 1;
      return;
    }

    size = 1 << position;
    BRN_DEBUG("Next is %s",next->_ether_addr.unparse().c_str());

    WritablePacket *p = DHTProtocolFalcon::new_nws_packet(_frt->_me, next, size);
    output(0).push(p);
  }
}

void
FalconNetworkSizeDetermination::handle_nws(Packet *packet)
{
  DHTnode next;
  DHTnode src;
  uint32_t size;
  uint8_t position;

  BRN_DEBUG("handle_nws");

  DHTProtocolFalcon::get_nws_info(packet, &src, &size);

  if ( src.equals(_frt->_me) ) {
    BRN_DEBUG("Final destination");
    _networksize = size;
    packet->kill();
  } else {
    DHTnode *route_next = get_responsibly_node_FT( src._md5_digest, &position);

    if ( route_next == NULL ) {
      BRN_ERROR("Didn't find responsible Node");
      packet->kill();
      return;
    }

    size = size + (1 << position);

    BRN_DEBUG("Next is %s",route_next->_ether_addr.unparse().c_str());

    WritablePacket *p = DHTProtocolFalcon::fwd_nws_packet(_frt->_me, route_next, size, packet);
    output(0).push(p);
  }
}

String
FalconNetworkSizeDetermination::networksize()
{
  StringAccum sa;

  sa << "Network size: " << _networksize;

  return sa.take_string();
}

enum {
  H_START_REQUEST,
  H_NETWORK_SIZE,
};

static String
read_param(Element *e, void *thunk)
{
  FalconNetworkSizeDetermination *nws = reinterpret_cast<FalconNetworkSizeDetermination *>(e);

  switch ((uintptr_t) thunk)
  {
    case H_NETWORK_SIZE : return ( nws->networksize() );
    default: return String();
  }
}

static int
write_param(const String &/*in_s*/, Element *e, void *, ErrorHandler */*errh*/)
{
  FalconNetworkSizeDetermination *nws = reinterpret_cast<FalconNetworkSizeDetermination *>(e);

  nws->request_nws();

  return 0;
}

void
FalconNetworkSizeDetermination::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("start_request", write_param , (void *)H_START_REQUEST);
  add_read_handler("networksize", read_param , (void *)H_NETWORK_SIZE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconNetworkSizeDetermination)
