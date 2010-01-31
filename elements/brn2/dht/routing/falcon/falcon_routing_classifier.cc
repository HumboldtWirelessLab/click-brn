#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routing_classifier.hh"
CLICK_DECLS

FalconRoutingClassifier::FalconRoutingClassifier():
  _debug(BrnLogger::DEFAULT)
{
}

FalconRoutingClassifier::~FalconRoutingClassifier()
{
}

int FalconRoutingClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconRoutingClassifier::initialize(ErrorHandler *)
{
  return 0;
}

void
FalconRoutingClassifier::push( int /*port*/, Packet *packet )
{
  if ( DHTProtocol::get_type(packet) == NWS_REQUEST ) {
    output(FALCON_NETWORKSIZE_PORT).push(packet);
    return;
  }

  switch ( DHTProtocolFalcon::get_operation(packet) ) {
    case FALCON_OPERATION_REQUEST_SUCCESSOR:
    case FALCON_OPERATION_UPDATE_SUCCESSOR:
      output(FALCON_ROUTING_SUCC_PORT).push(packet);
      break;
    case FALCON_OPERATION_REQUEST_POSITION:
      output(FALCON_ROUTING_POSITION_PORT).push(packet);
      break;
    default:
      BRN_WARN("Unknown Operation in falcon-classifier: %d", DHTProtocolFalcon::get_operation(packet));
      if ( noutputs() > FALCON_ROUTING_UNKNOWN_PORT )
        output(FALCON_ROUTING_UNKNOWN_PORT).push(packet);
      else
        packet->kill();
      break;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingClassifier)
