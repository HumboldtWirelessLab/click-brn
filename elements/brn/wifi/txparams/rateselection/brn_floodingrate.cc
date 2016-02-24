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
#include "elements/brn/standard/fixpointnumber.hh"
#include "rateselection.hh"
#include "brn_floodingrate.hh"

CLICK_DECLS

BrnFloodingRate::BrnFloodingRate()
 : _flooding(NULL),
   _fhelper(NULL),
   _flooding_db(NULL),
   _linkstat(NULL),
   _cst(NULL),
   _fl_rate_strategy(FLOODINGRATE_SINGLE_MAXRATE),
   mcs_zero(0),
   _dflt_retries(7),
   _no_pkts(0),
   _saved_power_sum(0)
{
  _default_strategy = RATESELECTION_FLOODING;
}

void *
BrnFloodingRate::cast(const char *name)
{
  if (strcmp(name, "BrnFloodingRate") == 0)
    return dynamic_cast<BrnFloodingRate *>(this);

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
    "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
    "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
    "CHANNELSTATS", cpkP , cpElement, &_cst,
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
BrnFloodingRate::process_feedback(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo * /*nri*/)
{
  click_wifi_extra *ceh = rs_pkt_info->ceh;

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
BrnFloodingRate::assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *nri)
{
  click_wifi_extra *ceh = rs_pkt_info->ceh;

  /* stats */
  _no_pkts++;

  /* rateselection */
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

      if ( nri->_eth.is_group() ) {
        ceh->max_tries = 1;
        ceh->power = nri->_max_power;
      } else {
        ceh->max_tries = _dflt_retries;
        ceh->power = get_min_power(nri->_eth);
      }

      break;
    case FLOODINGRATE_SINGLE_BEST_POWER_RATE:
      //TODO: Metric m^2*t (kleiner is besser)
      //Optimierung: was ist wie zu gewichten

      if ( nri->_eth.is_group() ) {
        ceh->max_tries = 1;
        nri->_rates[0].setWifiRate(ceh, 0);
        ceh->power = nri->_max_power;
      } else {
        ceh->max_tries = _dflt_retries;
        MCS mcs;
        ceh->power = get_best_rate_min_power(nri->_eth, &mcs);
        mcs.setWifiRate(ceh, 0);
      }
      break;
    case FLOODINGRATE_GROUP_MAXRATE: {
        Vector<EtherAddress> target_group;
        MCS mcs;

        get_group(&target_group);

        if ( get_best_rate_group(target_group, &mcs) == -1 ) {
          BRN_WARN("Error while getting best rate for %s",nri->_eth.unparse().c_str());
        }

        mcs.setWifiRate(ceh, 0);
        ceh->power = nri->_max_power;

        if ( nri->_eth.is_group() ) ceh->max_tries = 1;
        else ceh->max_tries = _dflt_retries;
      }
      break;
    case FLOODINGRATE_GROUP_MINPOWER: {
        if ( nri->_eth.is_group() ) ceh->max_tries = 1;
        else ceh->max_tries = _dflt_retries;

        nri->_rates[0].setWifiRate(ceh, 0);

        Vector<EtherAddress> target_group;
        get_group(&target_group);
        ceh->power = get_min_power_group(target_group);
      }
      break;
    case FLOODINGRATE_GROUP_BEST_POWER_RATE: {
        //TODO: Idea: power sinkt mit abstand^2, mit doppeltem abstand 
        if ( nri->_eth.is_group() ) ceh->max_tries = 1;
        else ceh->max_tries = _dflt_retries;

        Vector<EtherAddress> target_group;
        get_group(&target_group);

        MCS mcs;

        ceh->power = get_best_rate_min_power_group(target_group, &mcs);
        mcs.setWifiRate(ceh, 0);
      }
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
    rate.set_packed_16(pl->_probe_types[x]._rate);
    uint32_t effective_rate = rate._data_rate * (uint32_t)pl->_fwd_rates[x];

    BRN_DEBUG("Check Rate: %s (%d)",rate.to_string().c_str(), rate._data_rate);

    if ( effective_rate > max_effective_rate ) {
      max_effective_rate_index = x;
      max_effective_rate = effective_rate;
    }
  }

  if ( max_effective_rate_index == -1 ) max_effective_rate_index = 0;
  best_rate->set_packed_16(pl->_probe_types[max_effective_rate_index]._rate);

  return 0;
}

int
BrnFloodingRate::get_min_power(EtherAddress& ether)
{
  uint32_t min_power_pdr = 0;
  uint32_t min_power = 255;
  int min_power_pdr_index = -1;
  uint32_t min_rx_power = 255;

  BrnRateSize basic_rate = _linkstat->_ads_rs[0];
  BRN2LinkStat::probe_list_t *pl = _linkstat->_bcast_stats.findp(ether);

  if ( (pl == NULL) || (pl->_probe_types.size() == 0) ) return -1;

  for (int x = 0; x < pl->_probe_types.size(); x++) {
    if ( pl->_probe_types[x]._rate != basic_rate._rate ) continue;
    if (((uint32_t)pl->_fwd_rates[x] >= min_power_pdr) && ((uint32_t)pl->_probe_types[x]._power < min_power )) {
      min_power_pdr_index = x;
      min_power_pdr = (uint32_t)pl->_fwd_rates[x];
      min_power = (uint32_t)pl->_probe_types[x]._power;
      min_rx_power = (uint32_t)pl->_fwd_min_rx_powers[x];
    }
  }

  if ( min_power_pdr_index == -1 ) return -1;

  BRN_DEBUG("Min Power: %d (Basic rate: %d)",min_power, pl->_probe_types[min_power_pdr_index]._rate);

  BRN_DEBUG("Further improvements:");

  ChannelStats::RSSIInfo *rssi_info = _cst->get_rssi_summary();
  uint32_t min_rssi = rssi_info->min_rssi_per_rate[pl->_probe_types[min_power_pdr_index]._rate];
  int down_steps_db =  MAX(0,((int)min_rx_power-(int)min_rssi));

  BRN_DEBUG("TX: %d RX: %d For %d we need %d. so save %d", min_power, min_rx_power,
                                                               basic_rate._rate, min_rssi, down_steps_db);

  min_power -= down_steps_db;

  /* stats */
  _saved_power_sum += down_steps_db;

  return min_power;
}

int
BrnFloodingRate::get_best_rate_min_power(EtherAddress &ether, MCS *best_rate)
{
  uint32_t min_power = 255;
  uint32_t min_rx_power = 255;

  MCS rate;
  uint32_t max_effective_rate = 0;
  int max_effective_rate_index = -1;

  BRN2LinkStat::probe_list_t *pl = _linkstat->_bcast_stats.findp(ether);

  if ( (pl == NULL) || (pl->_probe_types.size() == 0) ) return -1;

  for (int x = 0; x < pl->_probe_types.size(); x++) {
    rate.set_packed_16(pl->_probe_types[x]._rate);
    uint32_t effective_rate = rate._data_rate * (uint32_t)pl->_fwd_rates[x];

    BRN_DEBUG("Check Rate: %s (%d)",rate.to_string().c_str(), rate._data_rate);

    if ( effective_rate > max_effective_rate ) {
      max_effective_rate_index = x;
      max_effective_rate = effective_rate;
      min_power = (uint32_t)pl->_probe_types[x]._power;
      min_rx_power = (uint32_t)pl->_fwd_min_rx_powers[x];
    }
  }

  BRN_DEBUG("Eff Rate: %d", rate._data_rate);

  ChannelStats::RSSIInfo *rssi_info = _cst->get_rssi_summary();
  uint32_t min_rssi = rssi_info->min_rssi_per_rate[rate._rate];
  int down_steps_db = ((int)min_rx_power-(int)min_rssi);

  BRN_DEBUG("TX: %d RX: %d For %d we need %d. so save %d", min_power, min_rx_power,
                                                               rate._rate, min_rssi, down_steps_db);

  min_power -= down_steps_db;

  if ( max_effective_rate_index == -1 ) max_effective_rate_index = 0;
  best_rate->set_packed_16(pl->_probe_types[max_effective_rate_index]._rate);

  /* stats */
  _saved_power_sum += down_steps_db;

  return min_power;
}

int
BrnFloodingRate::get_group_info(int mode, Vector<EtherAddress> &group, MCS *best_rate)
{
  BrnRateSize2EffectiveRate erate_map;
  BrnRateSize2RSSI power_map;
  BrnRateSize2Count rate_counter;

  int *curr_erate_p;

  MCS rate;

  BRN_DEBUG("Group: %d",group.size());

  for (int group_i = 0; group_i < group.size(); group_i++) {

    BRN2LinkStat::probe_list_t *pl = _linkstat->_bcast_stats.findp(group[group_i]);

    if ( (pl == NULL) || (pl->_probe_types.size() == 0) ) continue;

    for (int x = 0; x < pl->_probe_types.size(); x++) {
      BrnRateSize brs = BrnRateSize(pl->_probe_types[x]._rate, pl->_probe_types[x]._size, pl->_probe_types[x]._power);

      uint16_t probe_rate16 = pl->_probe_types[x]._rate;
      rate.set_packed_16(probe_rate16);

      uint32_t effective_rate = rate._data_rate * (uint32_t)pl->_fwd_rates[x];

      BRN_DEBUG("Check Rate: %s (%d)",rate.to_string().c_str(), rate._data_rate);

      if ( (curr_erate_p = erate_map.findp(brs)) == NULL ) {
        erate_map.insert(brs, effective_rate);
        power_map.insert(brs, (uint32_t)pl->_fwd_min_rx_powers[x]);
        rate_counter.insert(brs,1);
      } else {
        erate_map.insert(brs, MIN(*curr_erate_p,(int)effective_rate));                               //insert Minrate for group
        power_map.insert(brs, MIN(power_map.find(brs),(int)pl->_fwd_min_rx_powers[x]));
        *(rate_counter.findp(brs)) += 1;
      }
    }
  }

  if ( mode == FLOODINGRATE_GROUP_MAXRATE ) {
    uint32_t max_effective_rate = 0;
    uint16_t max_effective_rate_mcs16 = 0;

    BRN_DEBUG("Size: %d",erate_map.size());

    for (BrnRateSize2EffectiveRateIter i = erate_map.begin(); i.live();++i) {
      BrnRateSize brs = i.key();
      int effective_rate = i.value();
      int count = rate_counter.find(brs);

      BRN_DEBUG("%d %d %d",(uint32_t)brs._rate, effective_rate, count);

      if ((count == group.size()) && (effective_rate >= (int)max_effective_rate)) {
        max_effective_rate = effective_rate;
        max_effective_rate_mcs16 = brs._rate;
      }
    }

    if ( max_effective_rate == 0 ) {
      BRN_DEBUG("No rate for group");
      return 0;
    }

    best_rate->set_packed_16(max_effective_rate_mcs16);

    BRN_DEBUG("Group (%d) Rate: %s (%d)",group.size(),rate.to_string().c_str(), rate._data_rate);

  } else if ( mode == FLOODINGRATE_GROUP_MINPOWER ) {
    BrnRateSize basic_rate = _linkstat->_ads_rs[0];

    ChannelStats::RSSIInfo *rssi_info = _cst->get_rssi_summary();
    uint32_t min_rssi = rssi_info->min_rssi_per_rate[basic_rate._rate];
    int down_steps_db = MAX(0,((int)power_map.find(basic_rate)-(int)min_rssi));

    BRN_DEBUG("TX: %d RX: %d For %d we need %d. so save %d", (int)basic_rate._power, (int)power_map.find(basic_rate),
                                                              basic_rate._rate, min_rssi, down_steps_db);

    /* stats */
    _saved_power_sum += down_steps_db;

    return ((int)basic_rate._power - down_steps_db);
  } else if ( mode == FLOODINGRATE_GROUP_BEST_POWER_RATE ) {
    uint32_t max_effective_rate = 0;
    uint16_t max_effective_rate_mcs16 = 0;

    BRN_DEBUG("Size: %d",erate_map.size());

    for (BrnRateSize2EffectiveRateIter i = erate_map.begin(); i.live();++i) {
      BrnRateSize brs = i.key();
      int effective_rate = i.value();
      int count = rate_counter.find(brs);

      BRN_DEBUG("%d %d %d",(uint32_t)brs._rate, effective_rate, count);

      if ((count == group.size()) && (effective_rate >= (int)max_effective_rate)) {
        max_effective_rate = effective_rate;
        max_effective_rate_mcs16 = brs._rate;
      }
    }

    if ( max_effective_rate == 0 ) {
      BRN_DEBUG("No rate for group");
      return 0;
    }

    best_rate->set_packed_16(max_effective_rate_mcs16);

    BRN_DEBUG("Group (%d) Rate: %s (%d)",group.size(),rate.to_string().c_str(), rate._data_rate);
    BrnRateSize basic_rate = _linkstat->_ads_rs[0];

    ChannelStats::RSSIInfo *rssi_info = _cst->get_rssi_summary();
    uint32_t min_rssi = rssi_info->min_rssi_per_rate[max_effective_rate_mcs16];
    int down_steps_db = MAX(0,((int)power_map.find(basic_rate)-(int)min_rssi));

    BRN_DEBUG("TX: %d RX: %d For %d we need %d. so save %d", (int)basic_rate._power, (int)power_map.find(basic_rate),
                                                              basic_rate._rate, min_rssi, down_steps_db);

    /* stats */
    _saved_power_sum += down_steps_db;

    return ((int)basic_rate._power - down_steps_db);
  }
  return 0;
}

int
BrnFloodingRate::get_best_rate_group(Vector<EtherAddress> &group, MCS *best_rate)
{
  return get_group_info(FLOODINGRATE_GROUP_MAXRATE, group, best_rate);
}

int
BrnFloodingRate::get_min_power_group(Vector<EtherAddress> &group)
{
  return get_group_info(FLOODINGRATE_GROUP_MINPOWER, group, NULL);
}

int
BrnFloodingRate::get_best_rate_min_power_group(Vector<EtherAddress> &group, MCS *best_rate)
{
  return get_group_info(FLOODINGRATE_GROUP_BEST_POWER_RATE, group, best_rate);
}

/*
 *  d ~ power^0.5  //d nimm mit wurzel(power) ab, weil A von Kugel ~ d^2
 *  A ~ d^2
 *  -> A ~ d^2 ~ (power^0.5)^2 ~ power
 *
 */
int
BrnFloodingRate::metric_space_bits_per_second(MCS &/*rate*/, uint32_t /*tx_power*/)
{
  return 0;
}

void
BrnFloodingRate::get_group(Vector<EtherAddress> *group)
{
  uint16_t bcast_id;
  EtherAddress *bcast_src = _flooding->get_last_tx(&bcast_id);

  struct BroadcastNode::flooding_node_info *last_nodes;
  uint32_t last_nodes_size;
  last_nodes = _flooding_db->get_node_infos(bcast_src, bcast_id, &last_nodes_size);

  Vector<EtherAddress> target_group;

  for ( uint32_t j = 0; j < last_nodes_size; j++ ) {                               //add node to candidate set if
    if ( ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_RESPONSIBILITY) != 0) && //1. i'm responsible (due unicast before or fix set)
        ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED) == 0)) {         //2. is not acked
      group->push_back(EtherAddress(last_nodes[j].etheraddr));
    }
  }
}



String
BrnFloodingRate::print_neighbour_info(NeighbourRateInfo * /*nri*/, int /*tabs*/)
{
  StringAccum sa;

//  DstInfo *nfo = reinterpret_cast<DstInfo*>(nri->_rs_data);

  return sa.take_string();
}

String
BrnFloodingRate::print_stats(int /*tabs*/)
{
  StringAccum sa;

  FPN s = FPN(0);
  if ( _no_pkts > 0 ) s = FPN(_saved_power_sum)/FPN(_no_pkts);

  sa << "<floodingrateselection node=\"" << BRN_NODE_NAME << "\" no_pkts=\"" << _no_pkts;
  sa << "\" sum_saved_dbm=\""  << _saved_power_sum << "\" avg_saved_dbm=\""  << s << "\" >\n";

  sa << "</floodingrateselection>\n";

  return sa.take_string();
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

enum { H_STATS};


static String
BrnFloodingRate_read_param(Element *e, void *thunk)
{
  BrnFloodingRate *f = reinterpret_cast<BrnFloodingRate *>(e);
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return f->print_stats(0);
    default:
      return String();
  }
}

/*
static int
BrnFloodingRate_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  BrnFloodingRate *f = reinterpret_cast<BrnFloodingRate *>(e);
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
