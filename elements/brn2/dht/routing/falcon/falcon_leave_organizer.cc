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

#include "elements/brn2/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_leave_organizer.hh"

CLICK_DECLS

FalconLeaveOrganizer::FalconLeaveOrganizer():
  _lookup_timer(static_lookup_timer_hook,this),
  _debug(BrnLogger::DEFAULT)
{
}

FalconLeaveOrganizer::~FalconLeaveOrganizer()
{
}

int FalconLeaveOrganizer::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "RETRIES", cpkP, cpInteger, &_max_retries,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int FalconLeaveOrganizer::initialize(ErrorHandler *)
{
  _lookup_timer.initialize(this);
  return 0;
}

void
FalconLeaveOrganizer::static_lookup_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
}

void
FalconLeaveOrganizer::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) )
    packet->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FalconLeaveOrganizer)
