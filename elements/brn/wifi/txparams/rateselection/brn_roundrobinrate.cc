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
#include "brn_roundrobinrate.hh"

CLICK_DECLS

BrnRoundRobinRate::BrnRoundRobinRate():
 _tries(1)
{
  _default_strategy = RATESELECTION_ROUNDROBIN;
}

void *
BrnRoundRobinRate::cast(const char *name)
{
  if (strcmp(name, "BrnRoundRobinRate") == 0)
    return dynamic_cast<BrnRoundRobinRate *>(this);

  return RateSelection::cast(name);
}

BrnRoundRobinRate::~BrnRoundRobinRate()
{
}

int
BrnRoundRobinRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
      "TRIES", 0, cpByte, &_tries,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}


/************************************************************************************/
/*********************************** M A I N ****************************************/
/************************************************************************************/
void
BrnRoundRobinRate::assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *nri)
{
  click_wifi_extra *ceh = rs_pkt_info->ceh;

  if ( nri->_rs_data == NULL ) {
    nri->_rs_data = new uint32_t;
    *((uint32_t*)nri->_rs_data) = 0;
  }

  uint32_t *index = (uint32_t*)nri->_rs_data;
  (nri->_rates[*index]).setWifiRate(ceh,0);
  *index = (*index + 1) % nri->_rates.size();

  ceh->rate1 = 0;
  ceh->rate2 = 0;
  ceh->rate3 = 0;

  ceh->max_tries = _tries;
  ceh->max_tries1 = 0;
  ceh->max_tries2 = 0;
  ceh->max_tries3 = 0;

  return;
}

String
BrnRoundRobinRate::print_neighbour_info(NeighbourRateInfo * /*nri*/, int /*tabs*/)
{
  return String();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum { H_STATS};


static String
BrnRoundRobinRate_read_param(Element */*e*/, void *thunk)
{
  //BrnRoundRobinRate *td = reinterpret_cast<BrnRoundRobinRate *>(e);
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return String();
    default:
      return String();
  }
}

void
BrnRoundRobinRate::add_handlers()
{
  RateSelection::add_handlers();

  add_read_handler("stats", BrnRoundRobinRate_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnRoundRobinRate)

