#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <elements/wifi/availablerates.hh>

#include "elements/brn/brn2.h"
#include "rateselection.hh"
#include "brn_floodingrate.hh"

CLICK_DECLS

BrnFloodingRate::BrnFloodingRate()
 : _flooding(NULL),
   _fhelper(NULL)
{
  mcs_zero = MCS(0);
  _default_strategy = RATESELECTION_FLOODING;
}

void *
BrnFloodingRate::cast(const char *name)
{
  if (strcmp(name, "BrnFloodingRate") == 0)
    return (BrnFloodingRate *) this;
  else if (strcmp(name, "RateSelection") == 0)
    return (RateSelection *) this;
  else
    return NULL;
}

BrnFloodingRate::~BrnFloodingRate()
{
}

int
BrnFloodingRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int ret = Args(conf, this, errh)
      //.read("FLOODING", &_flooding
      //.read("FLOODINGHELPER", &_fhelper)
      .read("DEBUG", _debug)
      .complete();
  return ret;
}


/************************************************************************************/
/*********************************** M A I N ****************************************/
/************************************************************************************/

void
BrnFloodingRate::adjust_all(NeighborTable * /*neighbors*/)
{
}

void
BrnFloodingRate::adjust(NeighborTable * /*neighbors*/, EtherAddress /*dst*/)
{
}

void
BrnFloodingRate::process_feedback(click_wifi_extra *ceh, struct brn_click_wifi_extra_extention *, NeighbourRateInfo * /*nri*/)
{
  int no_transmissions = 1;
  int no_rts_transmissions = 0;

  if (ceh->flags & WIFI_EXTRA_TX_ABORT) no_transmissions = (int)ceh->retries;
  else no_transmissions = (int)ceh->retries + 1;

  if (ceh->flags & WIFI_EXTRA_EXT_RETRY_INFO) {
     no_rts_transmissions = (int)(ceh->virt_col >> 4);
     no_transmissions = (int)(ceh->virt_col & 15);
  }

  BRN_DEBUG("TX: %d RTS: %d",no_transmissions,no_rts_transmissions);
}

void
BrnFloodingRate::assign_rate(click_wifi_extra * /*ceh*/, struct brn_click_wifi_extra_extention *, NeighbourRateInfo * /*nri*/)
{
  return;
}

String
BrnFloodingRate::print_neighbour_info(NeighbourRateInfo * /*nri*/, int /*tabs*/)
{
  StringAccum sa;

//  DstInfo *nfo = (DstInfo*)nri->_rs_data;

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum { H_STATS};


static String
BrnFloodingRate_read_param(Element */*e*/, void *thunk)
{
  //BrnFloodingRate *td = (BrnFloodingRate *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return String();
    default:
      return String();
  }
}

/*
static int
BrnFloodingRate_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnFloodingRate *f = (BrnFloodingRate *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_INFO: {
    }
  }
  return 0;
}
*/
void
BrnFloodingRate::add_handlers()
{
  RateSelection::add_handlers();

  add_read_handler("stats", BrnFloodingRate_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnFloodingRate)

