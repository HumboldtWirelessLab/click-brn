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
   _fhelper(NULL),
   _fl_rate_strategy(FLOODINGRATE_SINGLE_MAXRATE),
   _dflt_retries(7)
{
  mcs_zero = MCS(0);
  _default_strategy = RATESELECTION_FLOODING;
}

void *
BrnFloodingRate::cast(const char *name)
{
  if (strcmp(name, "BrnFloodingRate") == 0)
    return (BrnFloodingRate *) this;

  return RateSelection::cast(name);
}

BrnFloodingRate::~BrnFloodingRate()
{
}

int
BrnFloodingRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "FLOODING", cpkP+cpkM, cpElement, &_flooding,
    "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
    "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
    "STRATEGY", cpkP, cpInteger, &_fl_rate_strategy,
    "DEFAULTRETRIES", cpkP, cpInteger, &_dflt_retries,
    "DEBUG", cpkP, cpInteger, &_debug,
  cpEnd) < 0) return -1;

  return 0;
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
BrnFloodingRate::assign_rate(click_wifi_extra *ceh, struct brn_click_wifi_extra_extention *, NeighbourRateInfo *nri)
{
  switch (_fl_rate_strategy) {
    case FLOODINGRATE_DEFAULT_VALUES:
      nri->_rates[0].setWifiRate(ceh, 0);

      if ( nri->_eth.is_group() ) ceh->max_tries = 1;
      else ceh->max_tries = _dflt_retries;

      ceh->power = nri->_max_power;

      break;
    case FLOODINGRATE_SINGLE_MAXRATE:
      if ( nri->_eth.is_group() ) {
        BRN_DEBUG("Group. use smallest rate");
        //nri->print_mcs_vector();
        nri->_rates[0].setWifiRate(ceh, 0);
        ceh->max_tries = 1;
      } else {
        MCS mcs;
        if ( get_best_rate(nri->_eth, &mcs) == -1 ) {
          BRN_WARN("Error while getting best rate for %s",nri->_eth.unparse().c_str());
        }
        mcs.setWifiRate(ceh, 0);
        ceh->max_tries = _dflt_retries;
      }
      ceh->power = nri->_max_power;
      break;
    case FLOODINGRATE_SINGLE_MINPOWER:
      nri->_rates[0].setWifiRate(ceh, 0);

      if ( nri->_eth.is_group() ) ceh->max_tries = 1;
      else ceh->max_tries = _dflt_retries;

      ceh->power = get_min_power(nri->_eth);

      break;
    case FLOODINGRATE_SINGLE_BEST_POWER_RATE:
      //TODO: Metric m^2*t (kleiner is besser)
      //Optimierung: was ist wie zu gewichten

      break;
    case FLOODINGRATE_GROUP_MAXRATE:
      break;
    case FLOODINGRATE_GROUP_MINPOWER:
      break;
    case FLOODINGRATE_GROUP_BEST_POWER_RATE:
      //TODO: Idea: power sinkt mit abstand^2, mit doppeltem abstand 

      break;
  }
  return;
}

int
BrnFloodingRate::get_best_rate(EtherAddress &ether, MCS *best_rate)
{
  MCS rate;
  uint32_t max_effective_rate = 0;
  int max_effective_rate_index = -1;

  BRN2LinkStat::probe_list_t *pl = _linkstat->_bcast_stats.findp(ether);

  if ( (pl == NULL) || (pl->_probe_types.size() == 0) ) return -1;

  for (int x = 0; x < pl->_probe_types.size(); x++) {

    BRN_DEBUG("Check Rate: %s (%d)",rate.to_string().c_str(), rate._data_rate);

    rate.set_packed_16(pl->_probe_types[x]._rate);
    uint32_t effective_rate = rate._data_rate * (uint32_t)pl->_fwd_rates[x];

    if ( effective_rate > max_effective_rate ) {
      max_effective_rate_index = x;
      max_effective_rate = effective_rate;
    }
  }

  if ( max_effective_rate_index == -1 ) max_effective_rate_index = 0;
  best_rate->set_packed_16(pl->_probe_types[max_effective_rate_index]._rate);

  return 0;
}

int get_min_power(EtherAddress& ether)
{

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

