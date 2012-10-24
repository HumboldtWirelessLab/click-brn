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
  switch ( DHTProtocol::get_type(packet) ) {
    case FALCON_MINOR_REQUEST_SUCCESSOR:
    case FALCON_MINOR_REPLY_SUCCESSOR:
    case FALCON_MINOR_UPDATE_SUCCESSOR:
      output(FALCON_ROUTING_SUCC_PORT).push(packet);
      break;
    case FALCON_MINOR_REQUEST_POSITION:
    case FALCON_MINOR_REPLY_POSITION:
      output(FALCON_ROUTING_POSITION_PORT).push(packet);
      break;
    case FALCON_MINOR_LEAVE_NETWORK_NOTIFICATION:
    case FALCON_MINOR_LEAVE_NETWORK_REPLY:
      output(FALCON_ROUTING_LEAVE_PORT).push(packet);
      break;
    case FALCON_MINOR_NWS_REQUEST:
      output(FALCON_NETWORKSIZE_PORT).push(packet);
      break;
    case FALCON_MINOR_PASSIVE_MONITORING_ACTIVATE:
      output(FALCON_PASSIVE_MONITOR_PORT).push(packet);
      break;
    default:
      BRN_WARN("Unknown Operation in falcon-classifier: %d", DHTProtocol::get_type(packet));
      if ( noutputs() > FALCON_ROUTING_UNKNOWN_PORT )
        output(FALCON_ROUTING_UNKNOWN_PORT).push(packet);
      else
        packet->kill();
      break;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingClassifier)
