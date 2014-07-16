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

#define ANN_SPEC "input 3 1 1 fully_connected 4 0 0.05 1 fully_connected 4 0 0.05 1 "\
                 "fully_connected 2 0 0.05 1 output 1 0 0.05 1 error_function 1 "\
                 "parameters    16.3458\n-0.0754721\n  -6.22471\n    3.1956\n   "\
                 "37.9858\n-0.0419361\n   -11.659\n   7.67339\n 0.0375283\n"\
                 "-0.0707572\n  -2.02023\n 0.0065733\n    5.5004\n 0.0135251\n"\
                 "  -2.96058\n   1.12907\n0.00184868\n   48.0301\n  0.229655\n"\
                 "0.00405547\n  -48.1953\n   3.91757\n  -66.7874\n -0.294909\n "\
                 " 0.221874\n -0.195333\n   1.20757\n   9.90241\n  0.225474\n"\
                 "   1.14452\n  -4.40303\n   4.19181\n  -16.3558\n  0.169871\n"\
                 "  -1.07266\n   5.39329\n   39.3853\n   -38.892\n  -11.8951\n"\
                 "   23.9987\n  -6.54092\n   45.6367\n  -45.0305\n  -13.9749\n"\
                 "   27.6089\n    -7.291\n  -52.5025\n  -62.0485\n   48.0301"

CLICK_DECLS


BrnAnnRate::BrnAnnRate()
{
  _default_strategy = RATESELECTION_FIXRATE;
  String s(ANN_SPEC);
  _ann = new BrnAnnRateNet(s);
}

void *
BrnAnnRate::cast(const char *name)
{
	if (strcmp(name, "BrnAnnRate") == 0)
		return (BrnAnnRate *) this;

	return RateSelection::cast(name);
}

BrnAnnRate::~BrnAnnRate()
{
  delete(_ann);
}

int
BrnAnnRate::configure(Vector<String> &conf, ErrorHandler *errh)
{
	std::string str;
	if (cp_va_kparse(conf, this, errh,
    "HIDDENNODE", cpkM, cpElement, &_hnd,
    "LINKSTAT", cpkM , cpElement, &_linkstat,
    "DEBUG", 0, cpInteger, &_debug,
	  cpEnd) < 0)
		
	{
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
    /* 
    NeighbourRateInfo *nri = iter.value();
    setMinstrelInfo(nri);
     *  todo replace this lines
     */
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
  uint8_t min_rx_power = 255;
  BRN2LinkStat::probe_list_t *pl = _linkstat->_bcast_stats.findp(nri->_eth); // nri->_eth?

  if ( (pl == NULL) || (pl->_probe_types.size() == 0) ) 
  {
    BRN_DEBUG("ANN selection error: probelist empty"); 
    return;
  }

  for (int x = 0; x < pl->_probe_types.size(); x++) {
    if (((uint32_t)pl->_fwd_min_rx_powers[x] < min_rx_power )) {
      min_power_pdr_index = x;
      min_rx_power = (uint32_t)pl->_fwd_min_rx_powers[x];
    }
  }

  if ( min_power_pdr_index == -1 ) 
  {
    BRN_DEBUG("ANN selection error: no entry in probelist found"); 
    return;
  }
  
  int rssi = min_rx_power;
  
  // get Neighbours
  int neighbours = _linkstat->_neighbors.size();
  
  // get HiddenNodes
  int hiddennodes = _hnd->count_hidden_neighbours(nri->_eth);
  
  
  ///////////////////////////////////////////////////
  // get rate from ANN
  
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

enum { H_STATS, H_TEST};


static String
BrnAnnRate_read_param(Element */*e*/, void *thunk)
{
  //BrnAnnRate *td = (BrnAnnRate *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return String();
    
    case H_TEST:
    {
      String ann = "input 3 1 1 fully_connected 4 0 0.05 1 fully_connected 4 0 0.05 1 fully_connected 2 0 0.05 1 output 1 0 0.05 1 error_function 1 parameters    16.3458\n-0.0754721\n  -6.22471\n    3.1956\n   37.9858\n-0.0419361\n   -11.659\n   7.67339\n 0.0375283\n-0.0707572\n  -2.02023\n 0.0065733\n    5.5004\n 0.0135251\n  -2.96058\n   1.12907\n0.00184868\n   48.0301\n  0.229655\n0.00405547\n  -48.1953\n   3.91757\n  -66.7874\n -0.294909\n  0.221874\n -0.195333\n   1.20757\n   9.90241\n  0.225474\n   1.14452\n  -4.40303\n   4.19181\n  -16.3558\n  0.169871\n  -1.07266\n   5.39329\n   39.3853\n   -38.892\n  -11.8951\n   23.9987\n  -6.54092\n   45.6367\n  -45.0305\n  -13.9749\n   27.6089\n    -7.291\n  -52.5025\n  -62.0485\n   48.0301";
      BrnAnnRateNet rateNet(ann);
      return(rateNet.test());
    }
    
    default:
      return String();
  }
}

void
BrnAnnRate::add_handlers()
{
  RateSelection::add_handlers();

  add_read_handler("stats", BrnAnnRate_read_param, (void *) H_STATS);
  add_read_handler("test_net", BrnAnnRate_read_param, (void *) H_TEST);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnAnnRate)
ELEMENT_LIBS(-lopenann)

