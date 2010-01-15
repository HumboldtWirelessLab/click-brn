#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn2/standard/md5.h"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"
#include "dhtrouting_falcon.hh"
#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable_maintenance.hh"

CLICK_DECLS

FalconRoutingTableMaintenance::FalconRoutingTableMaintenance()
{
}

FalconRoutingTableMaintenance::~FalconRoutingTableMaintenance()
{
}

int FalconRoutingTableMaintenance::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconRoutingTableMaintenance::initialize(ErrorHandler *)
{
  return 0;
}

void FalconRoutingTableMaintenance::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) output(0).push(packet);
}

/*void
    DHTRoutingFalcon::nodeDetection()
{
  Vector<EtherAddress> neighbors;                       // actual neighbors from linkstat/neighborlist
  WritablePacket *p,*big_p;

  if ( (_linkstat == NULL) && (_fingertable.size() == 0) ) {  //active braodcast probing if no linkstat

    return;
  }

  _linkstat->get_neighbors(&neighbors);

  //Check for new neighbors
  for( int i = 0; i < neighbors.size(); i++ ) {
    click_chatter("New neighbors");

    p = DHTProtocolFalcon::new_route_request_packet(_me);
    big_p = DHTProtocol::push_brn_ether_header(p, &(_me->_ether_addr), &(neighbors[i]), BRN_PORT_DHTROUTING);

    if ( big_p == NULL ) click_chatter("Error in DHT");
    else output(0).push(big_p);
  }
}
*/
CLICK_ENDDECLS
EXPORT_ELEMENT(FalconRoutingTableMaintenance)
