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
#include "brn_annrate.hh"

CLICK_DECLS


BrnAnnRate::BrnAnnRate()
{
  _default_strategy = RATESELECTION_ANN; // what is this?
}

void *
BrnAnnRate::cast(const char *name)
{
	if (strcmp(name, "BrnAnnRate") == 0)
		return (BrnMinstrelRate *) this;

	return RateSelection::cast(name);
}

BrnAnnRate::~BrnAnnRate()
{
}

int
BrnAnnRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
	std::string str;
	if (cp_va_kparse(conf, this, errh,
    "HIDDENNODE", cpkP+cpkM, cpElement, &_hnd,
    "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
    //"CHANNELSTATS", cpkP , cpElement, &_cst,
    "DEBUG", cpkP, cpInteger, &_debug,
	cpEnd) < 0)
		
	{
		_ann = new BrnAnnRateNet(str);
		return -1;
	}

	return 0;
}

/************************************************************************************/
/********************************** H E L P E R *************************************/
/************************************************************************************/
const int rates[] = {6, 9, 12, 18, 24, 36, 48, 54};

int
BrnAnnRate::rate_to_index(uint8_t rate_mbits)
{
	for (int i = 0; i < 8; i++)
	{
		if (rates[i] == rate_mbits) return i;
	}
	
	return -1; // what and why?
}

/************************************************************************************/
/*********************************** M A I N ****************************************/
/************************************************************************************/

void
BrnAnnRate::adjust_all(NeighborTable *neighbors)
{
  for (NIter iter = neighbors->begin(); iter.live(); iter++) {
    NeighbourRateInfo *nri = iter.value();
    setMinstrelInfo(nri);
  }
}

void
BrnAnnRate::process_feedback(struct rateselection_packet_info */*rs_pkt_info*/, NeighbourRateInfo * /*nri*/)
{
  return;
}

void
BrnAnnRate::assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *nri)
{
  click_wifi_extra *eh = rs_pkt_info->ceh;
  
  // get RSSI 
  int min_power_pdr_index = -1;
  int min_rx_power = 9999;
  BRN2LinkStat::probe_list_t *pl = _linkstat->_bcast_stats.findp(_nri->_eth); // _nri->_eth?

  if ( (pl == NULL) || (pl->_probe_types.size() == 0) ) {BRN_DEBUG("ANN selection error: probelist empty"); return}

  for (int x = 0; x < pl->_probe_types.size(); x++) {
    if (((uint32_t)pl->_probe_types[x]._fwd_min_rx_powers[x] < min_rx_power )) {
      min_power_pdr_index = x;
      min_rx_power = (uint32_t)pl->_fwd_min_rx_powers[x];
    }
  }

  if ( min_power_pdr_index == -1 ) {BRN_DEBUG("ANN selection error: no entry in probelist found"); return}
  
  int rssi = min_rx_power;
  
  // get Neighbours
  int neighbours = _linkstat->_neighbours.size();
  
  // get HiddenNodes
  int hiddennodes = _hnd->count_hidden_neighbours(nri->_eth);
  
  
  ///////////////////////////////////////////////////
  // get rate from petri net
  
  uint8_t rate = _ann->getRate(neighbours, hiddennodes, rssi);
  
  
  
  ////////////////////////////////////////////////////
  // set determined rate
  
  // get rate number:
  int rate_index = rate_to_index(rate);
  
  MCS sample_mcs = nri->_rates[rate_index];
  sample_mcs.setWifiRate(eh,0);
  eh->max_tries = 7;
  
  // zero other rates
  eh->rate1 = 0;
  eh->rate2 = 0;
  eh->rate3 = 0;

  eh->max_tries1 = 0;
  eh->max_tries2 = 0;
  eh->max_tries3 = 0;
 
  return;
}



String
BrnAnnRate::print_neighbour_info(NeighbourRateInfo * /*nri*/, int /*tabs*/)
{
  StringAccum sa;

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

void
BrnAnnRate::add_handlers()
{
  RateSelection::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnAnnRate)
ELEMENT_LIBS(-lopenann)

