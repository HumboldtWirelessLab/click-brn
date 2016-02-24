#include "packetlossestimator.hh"

CLICK_DECLS

int get_acks_by_node(const EtherAddress &)
{
  return 0;
}

PacketLossEstimator::PacketLossEstimator() :
  _max_weak_signal_value(0),
  _dev(NULL),
  _cst(NULL),
  _cocst(NULL),
  _cinfo(NULL),
  _hnd(NULL),
  _pli(NULL),
  _pessimistic_hn_detection(false)

{
  BRNElement::init();
}

PacketLossEstimator::~PacketLossEstimator()
{
}

int PacketLossEstimator::configure(Vector<String> &conf, ErrorHandler* errh)
{
    int ret = cp_va_kparse(conf, this, errh,
                           "PLI", cpkP+cpkM, cpElement, &_pli,
                           "DEVICE", cpkP+cpkM, cpElement, &_dev,
                           "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
                           "COOPCHANNELSTATS", cpkP, cpElement, &_cocst,
                           "COLLISIONINFO", cpkP, cpElement, &_cinfo,
                           "HIDDENNODE", cpkP, cpElement, &_hnd,
                           "HNWORST", cpkP, cpBool, &_pessimistic_hn_detection,
                           "DEBUG", cpkP, cpInteger, &_debug,
                           cpEnd );

    return ret;
}

Packet *PacketLossEstimator::simple_action(Packet *packet)
{
  BRN_DEBUG("Packet *PacketLossEstimator::simple_action(Packet &packet)");

  if (packet == NULL) {
    BRN_ERROR("Packet is NULL");
    return NULL;
  }

  /** ------------- Get Packetinfo ----------------- **/
  struct click_wifi *w = (struct click_wifi *) packet->data();

  //click_chatter("Duration: %d", w->i_dur);
  int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;

  EtherAddress src;
  EtherAddress dst = EtherAddress(w->i_addr1);
  const EtherAddress *my_addr = _dev->getEtherAddress();

  switch (type) {
    case WIFI_FC0_TYPE_CTL:
      src = brn_etheraddress_broadcast;
      //if ((w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) == FC0_SUBTYPE_ACK) add_ack();
      break;
    case WIFI_FC0_TYPE_DATA:
    case WIFI_FC0_TYPE_MGT:
    default:
      src = EtherAddress(w->i_addr2);
      break;
  }

  /** ------ add node to packetlossinformation-DB --------- **/
  if(!_pli->mac_address_exists(src) && src != *my_addr && !src.is_broadcast()) _pli->graph_insert(src);

  if(!_pli->mac_address_exists(dst) && dst != *my_addr && !dst.is_broadcast()) _pli->graph_insert(dst);

  /** ------------- Update Stats ----------------- **/

  struct airtime_stats *stats = _cst->get_latest_stats();
  HashMap<EtherAddress, ChannelStats::SrcInfo*> *src_tab = _cst->get_latest_stats_neighbours();
  ChannelStats::SrcInfo *src_info = src_tab->find(src);
  ChannelStats::RSSIInfo *rssi_info = _cst->get_latest_rssi_info();

  if ( src != *my_addr ) {
    estimateWeakSignal(src, dst, src_info, rssi_info);
    estimateNonWifi(src, dst, stats);

    if ((_cinfo != NULL) && !src.is_broadcast()) {

      //CollisionInfo::RetryStatsTable rs_tab = _cinfo->rs_tab;
      //CollisionInfo::RetryStats *rs = rs_tab.find(src);

      // Backoff-Fraction
      // just output of fraction per queue (rs->get_frac(0))
    }

    if (_hnd != NULL) {
      //src has maybe neighbours, so update hidden node
      estimateHiddenNode(src, dst);
      if (!src.is_broadcast()) estimateInrange(src, dst);
    }

    if (!src.is_broadcast() && (src != *my_addr)) {
      if ( _received_adrs.findp(src) == NULL ) _received_adrs.insert(src,Timestamp::now());
    }
  }

  return packet;
}

void PacketLossEstimator::estimateHiddenNode(EtherAddress &src, EtherAddress &dst)
{
  BRN_DEBUG("void PacketLossEstimator::estimateHiddenNode()");

  if ( dst.is_broadcast() || src.is_broadcast() ||
      (dst == *_dev->getEtherAddress()) ||
      (src == *_dev->getEtherAddress())) return;

    if (_hnd->has_neighbours(src)) {   // does the source has neighbours at all?
      uint64_t hnProp = 0;
      bool coopst = false;

      BRN_INFO("%s has neighbours", src.unparse().c_str());

      uint8_t number_of_neighbours = 0;
      HiddenNodeDetection::NodeInfo *neighbour = _hnd->get_nodeinfo_table()->find(src);
      HiddenNodeDetection::NodeInfoTable *other_neighbour_list = neighbour->get_links_to();

      HiddenNodeDetection::NodeInfoTableIter other_neighbour_list_end = other_neighbour_list->end();
      HiddenNodeDetection::NodeInfoTableIter i = other_neighbour_list->begin();

      for (; i != other_neighbour_list_end; ++i) {
          EtherAddress tmpAdd = i.key();
          BRN_INFO("Neighbours Neighbour is: %s", tmpAdd.unparse().c_str());

          if(!_hnd->get_nodeinfo_table()->find(tmpAdd) || !_hnd->get_nodeinfo_table()->find(tmpAdd)->_neighbour) {
              BRN_INFO("Not in my Neighbour List");

              if ( number_of_neighbours >= 255 ) number_of_neighbours = 255;
              else                               number_of_neighbours++;
          } else {

              if (_received_adrs.findp(tmpAdd) != NULL) {
                  BRN_INFO("In my Neighbour List: %s, last seen: %d", tmpAdd.unparse().c_str(),
                                                                      _received_adrs.findp(tmpAdd)->unparse().c_str());

                  if ((( Timestamp::now().sec() - _received_adrs.find(tmpAdd).sec()) > 9) //TODO: why 9?
                         && get_acks_by_node(tmpAdd) > 0) {
                      if(number_of_neighbours >= 255) number_of_neighbours = 255;
                      else number_of_neighbours++;
                  }
              }
          }
      }

      if (_cocst != NULL) {
          NodeChannelStats *ncst = _cocst->get_stats(&src);
          NeighbourStatsMap *nats_map = ncst->get_last_neighbour_map();

          if (!nats_map->empty()) {

              uint32_t duration = 0;

              for(NeighbourStatsMapIter i = nats_map->begin(); i != nats_map->end(); ++i) {

                  EtherAddress tmpAdd = i.key();
                  struct neighbour_airtime_stats* nats = i.value();

                  BRN_INFO("EA: %s", tmpAdd.unparse().c_str());
                  BRN_INFO("NATS-EA: %s", EtherAddress(nats->_etheraddr).unparse().c_str());
                  BRN_INFO("PKT-Count: %d", nats->_rx_pkt_count);
                  BRN_INFO("Duration: %d", nats->_duration);

                  if (!_hnd->get_nodeinfo_table()->find(tmpAdd) || !_hnd->get_nodeinfo_table()->find(tmpAdd)->_neighbour)
                    duration += nats->_duration;
              }

              if (duration != 0) {
                  //TODO: fix Duration is Time (ms?)
                  hnProp = (duration * 100) / 1048576; /* 1024 * 1024 = 1Megab*/
                  coopst = true;
              }
          }
      }

      if (!coopst) {
          if (_pessimistic_hn_detection && number_of_neighbours > 0) { // pessimistic estimator, if we do have one hidden node, he will send as often as possible and as slow as possible. The may interrupt all of our transmissions
            hnProp = 100;
          } else {
              for(HiddenNodeDetection::NodeInfoTableIter i = other_neighbour_list->begin(); i != other_neighbour_list_end; ++i) {

                  EtherAddress tmpAdd = i.key();
                  BRN_INFO("Neighbours Neighbour is: %s", tmpAdd.unparse().c_str());

                  if(!_hnd->get_nodeinfo_table()->find(tmpAdd) || !_hnd->get_nodeinfo_table()->find(tmpAdd)->_neighbour)
                  {
                      BRN_INFO("Not in my Neighbour List");

                      //hnProp += get_acks_by_node(dst) * 12 / 10; // 12 ms per packet / 1000 ms * 100
                      //hnProp += get_acks_by_node(dst) * 12.5 / 10;
                      hnProp += get_acks_by_node(tmpAdd) * 12500 / 10; //ns
                  } else if ((_received_adrs.findp(tmpAdd) != NULL) &&
                             ((Timestamp::now().sec() - _received_adrs.find(tmpAdd).sec()) > 9) &&
                             get_acks_by_node(tmpAdd) > 0) {
                      BRN_INFO("%s seems to be gone, but is still there, i've seen %d acks!", tmpAdd.unparse().c_str(), get_acks_by_node(tmpAdd));
                      hnProp += get_acks_by_node(tmpAdd) * 12500 / 10;
                  } else {
                      BRN_INFO("%s is a direct neighbour", tmpAdd.unparse().c_str());
                      if (_hnd->get_nodeinfo_table()->find(tmpAdd)->_neighbour) {
                          BRN_INFO("HND_NEIGHBOUR");
                      }
                  }
              }
              hnProp /= 1000;
          }

          BRN_INFO("%d Acks received for %s", get_acks_by_node(dst), dst.unparse().c_str());
      }

      _pli->graph_get(src)->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);

      BRN_INFO(";;;;;;;;hnProp for %s: %i", src.unparse().c_str(), hnProp);
  }
}

void PacketLossEstimator::estimateInrange(EtherAddress &src, EtherAddress &)
{
    BRN_DEBUG("void PacketLossEstimator::estimateInrange()");
    if (!src.is_broadcast() && (src != *_dev->getEtherAddress())) return;

    uint8_t neighbours = 1;
    uint8_t irProp = 0;
    HiddenNodeDetection::NodeInfoTable *neighbour_nodes = _hnd->get_nodeinfo_table();

    for (HiddenNodeDetection::NodeInfoTableIter i = neighbour_nodes->begin(); i != neighbour_nodes->end(); ++i) {

        if (_hnd->get_nodeinfo_table()->find(i.key())->_neighbour) { // if entry is not a direct neighbour it should not be counted for inrange
            if(neighbours >= 255) neighbours = 255;
            else neighbours++;
        }
    }

    uint16_t backoffsize = _dev->get_cwmax()[0];

    if (backoffsize == 0) {
        BRN_ERROR("Can't get CWMax");
        return;
    }

    if (neighbours >= backoffsize) {
        irProp = 100;
    } else {
        uint64_t temp_a = 1;
        uint64_t temp_b = 1;

        for(int i = 1; i < neighbours; i++) {
            //temp *= (backoffsize - i) / backoffsize;
            if (temp_b > 429496) { //TODO: 429496 ??? MAX_INT*10000??
                temp_a /= 1000;
                temp_b /= 1000;
            }

            temp_a *= backoffsize - i;
            temp_b *= backoffsize;
        }

        irProp = (100 - (temp_a * 100 / temp_b));
    }

    _pli->graph_get(src)->reason_get(PacketLossReason::IN_RANGE)->setFraction(irProp);

    BRN_INFO("Backoff/Neighbours/In-Range for %s: %d/%d/%d", src.unparse().c_str(), backoffsize, neighbours, irProp);

}

void PacketLossEstimator::estimateNonWifi(EtherAddress &src, EtherAddress &/*dst*/, struct airtime_stats *ats)
{
    BRN_DEBUG("void PacketLossEstimator::estimateNonWifi(struct airtime_stats *ats)");
    if(!src.is_broadcast() && (src != *_dev->getEtherAddress())) return;

    uint8_t non_wifi = 1;
    if (ats->frac_mac_busy <= ats->hw_busy) {
        non_wifi = ats->hw_busy - ats->frac_mac_busy;
    }

    if (_pli->graph_get(src)) {
      _pli->graph_get(src)->reason_get(PacketLossReason::NON_WIFI)->setFraction(non_wifi);
      BRN_INFO("hw busy/mac busy/Non-Wifi: %d/%d/%d", ats->hw_busy, ats->frac_mac_busy, non_wifi);
    } else {
      BRN_WARN("No graph for src: %s",src.unparse().c_str());
    }
}

void PacketLossEstimator::estimateWeakSignal(EtherAddress &src, EtherAddress &/*dst*/, ChannelStats::SrcInfo *src_info, ChannelStats::RSSIInfo *rssi_info)
{
    if (!src.is_broadcast() && (src != *_dev->getEtherAddress())) return;

    if ( (src_info == NULL) || (rssi_info == NULL)) return;

    int8_t weaksignal = 100;

    if (src_info->_rssi > 0) weaksignal = calc_weak_signal_percentage(src_info, rssi_info);

    HiddenNodeDetection::NodeInfoTable *hnd_info_tab = _hnd->get_nodeinfo_table();

    for(int cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
        EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);

        if (hnd_info_tab->find(ea) != NULL && !hnd_info_tab->find(ea)->_neighbour) {

            _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(100);

        } else if (ea != src) {
            int last_ws = _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction();

            if(last_ws >= 10) {
                _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction((last_ws + last_ws - 10) / 2);
            } else {
                _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(0);
            }
        }
    }

    _pli->graph_get(src)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(weaksignal);
    BRN_INFO("RSSI/Weak Signal for %s: %d/%d", src.unparse().c_str(), src_info->_avg_rssi, weaksignal);

}

uint8_t PacketLossEstimator::calc_weak_signal_percentage(ChannelStats::SrcInfo *src_info, ChannelStats::RSSIInfo *rssi_info)
{
    uint32_t rssi_hist_index = src_info->_rssi_hist_index;
    uint8_t result = 0;
    uint8_t histogram[256];
    int32_t norm_histogram[256];
    int32_t expected_value = 0;
    int32_t variance = 0;
    int32_t std_deviation = 0;

    memset(histogram, 0, sizeof(histogram));
    memset(norm_histogram, 0, sizeof(norm_histogram));

    if (rssi_info == NULL) BRN_INFO("rssi_info NULL");

    /*
     * go through the history and create a real histogram, because we get only 50 values in any order
     */

    for (uint8_t i = 0; i < rssi_hist_index; i++) histogram[src_info->_rssi_hist[i]]++;

    /*
     * create normalized histogram
     * calculate expected value and variance
     */

    for (uint16_t k = 0; k < 256; k++) {
        if(histogram[k] == 0) continue;

        //todo: 1000000? Float to "fix"-point?
        norm_histogram[k] = (histogram[k] * 1000000) / rssi_hist_index;
        expected_value += (k * norm_histogram[k]);

        BRN_INFO("Hist[%d]: %d / Norm-Hist[%d]: %d/ Old: %f", k, histogram[k], k, norm_histogram[k],
                                                              float(histogram[k]) / float(rssi_hist_index));
    }

    BRN_INFO("Expected value: %d", expected_value);

    for (uint16_t l = 0; l < 256; l++) {
        if(norm_histogram[l] == 0) continue;

        //variance += (float(l) - expected_value) * (float(l) - expected_value) * norm_histogram[l];
        variance += ((((l * 1000000) - expected_value) / 1000000) * (((l * 1000000) - expected_value) / 1000000) * norm_histogram[l]);
    }

//    std_deviation = sqrtf(variance);
    std_deviation = isqrt32(variance) * 1000;

    BRN_INFO("Variance: %d / Standard deviation: %d", variance, std_deviation);

    if ((expected_value - std_deviation) <= 0) result = 100;
    else if((expected_value - std_deviation * 2) <= 0) result = 50;
    else if((expected_value - std_deviation * 3) <= 0) result = 25;
    else result = 0;

    /*
     * find min, max and hightest values
     */
    /*
    min[min_counter] = highest[highest_counter] = max[max_counter] = histogram[0];
    for (uint16_t j = 0; j < 256; j++) {
      //BRN_INFO("histogramm %d: %d", j, histogram[j]);
      if (min[min_counter] < histogram[j]) {
        if (min_counter <= highest_counter) ++min_counter;
      } else {
        if (j < 255) {
          min[min_counter] = histogram[j];
          min_val[min_counter] = j;
        } else min_val[min_counter] = 0;
      }

      if (highest[highest_counter] < histogram[j]) {
        highest[highest_counter] = histogram[j];
        max[highest_counter] = histogram[j];
        highest_val[highest_counter] = j;
        max_val[highest_counter] = j;
      } else {
        if (min_counter > highest_counter) highest_counter = min_counter;
      }

      if(max[max_counter] > histogram[j] && highest[max_counter] != 0 && max[max_counter]) {
        max[max_counter] = histogram[j];
        max_val[max_counter] = j;
      } else {
       if(max_counter < highest_counter) max_counter = highest_counter;
      }
    }

    //BRN_INFO("histogramm END");
    BRN_INFO("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[0], highest_val[0], max_val[0]);
    BRN_INFO("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[1], highest_val[1], max_val[1]);
    BRN_INFO("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[2], highest_val[2], max_val[2]);
    BRN_INFO("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[3], highest_val[3], max_val[3]);
    BRN_INFO("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[4], highest_val[4], max_val[4]);
    BRN_INFO("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[5], highest_val[5], max_val[5]);

    uint8_t highval = max_val[0] - highest_val[0];

    if(highest_val[0] - highval < 1) {
      if((highval - highest_val[0] - min_val[0]) * 2 < highval) result = 100;
      else result = 50;
    } else {
     result = 0;
    }*/
    return result;
}

/*
inline void PacketLossEstimator::add_ack(const EtherAddress &dest_addr)
{
    BRN_DEBUG("void PacketLossEstimator::add_ack(EtherAddress dest_addr)");
    uint32_t acks = PacketLossEstimator::_acks_by_node.find(dest_addr);
    PacketLossEstimator::_acks_by_node.erase(dest_addr);
    PacketLossEstimator::_acks_by_node.insert(dest_addr, ++acks);
}

inline uint32_t PacketLossEstimator::get_acks_by_node(const EtherAddress &dest_addr)
{
    BRN_DEBUG("uint32_t PacketLossEstimator::get_acks_by_node(EtherAddress dest_addr)");
    return PacketLossEstimator::_acks_by_node.find(dest_addr);
}

inline void PacketLossEstimator::reset_acks()
{
    for(HashMap<EtherAddress, uint32_t>::iterator i = PacketLossEstimator::_acks_by_node.begin(); i != PacketLossEstimator::_acks_by_node.end(); i++)
    {
        i.value() = 0;
    }
}
*/
////////////////////////// STATS //////////////////////////////

enum
{
    H_STATS
};

String PacketLossEstimator::stats_handler(int /*mode*/)
{
    BRN_INFO("String PacketLossEstimator::stats_handler");

    StringAccum sa;

    sa << "<packetlossreason" << " node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now() << "\">\n";

    if(_hnd != NULL) {
            HiddenNodeDetection::NodeInfoTable *hnd_info_tab = _hnd->get_nodeinfo_table();

            update_statistics(hnd_info_tab, _pli);
            sa << stats_get_hidden_node(hnd_info_tab, _pli);
            sa << stats_get_inrange(hnd_info_tab, _pli);
            sa << stats_get_weak_signal(hnd_info_tab, _pli);
            sa << stats_get_non_wifi(hnd_info_tab, _pli);
    }

    sa << "</packetlossreason>\n";

    return sa.take_string();
}

StringAccum PacketLossEstimator::stats_get_hidden_node(HiddenNodeDetection::NodeInfoTable *hnd_info_tab, PacketLossInformation *_pli)
{
    BRN_INFO("StringAccum PacketLossEstimator::stats_get_hidden_node");

    StringAccum hidden_node_sa;
    const int neighbour_no = hnd_info_tab->size();
    Vector<EtherAddress> neighbours;
    int cou = 0;

    for(HiddenNodeDetection::NodeInfoTableIter i = hnd_info_tab->begin(); i != hnd_info_tab->end() && cou < neighbour_no; ++i, cou++) {
        neighbours.push_back(i.key());
    }

    hidden_node_sa << "\t<hiddennodes observation_period=\"short\">\n";

    for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++)
    {
        EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);

        if (ea.is_broadcast() || hnd_info_tab->find(ea) == NULL || !hnd_info_tab->find(ea)->_neighbour || _pli->graph_get(ea) == NULL
                || (_received_adrs.findp(ea) == NULL)) continue;

        hidden_node_sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
        hidden_node_sa << "\t\t\t<fraction>" << _pli->graph_get(ea)->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction() << "</fraction>\n";

        //if(_cocst != NULL && !_cocst->get_stats(&ea).empty())
        if (_cocst != NULL) {
            HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map;// = _cocst->get_stats(&ea);

            for(HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator nats_map_iter = nats_map.begin(); nats_map_iter.live(); ++nats_map_iter)
            {
                const EtherAddress ea = nats_map_iter.key();

                if(*_dev->getEtherAddress() == ea) continue;
                if(_hnd == NULL) break;

                if(_hnd->get_nodeinfo_table()->find(ea) != NULL && _hnd->get_nodeinfo_table()->find(ea)->_neighbour) continue;

                hidden_node_sa << "\t\t\t<hidden_neighbours available=\"true\">\n";
                hidden_node_sa << "\t\t\t\t<address>" << ea.unparse().c_str() << "</address>\n";
                hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
            }
        } else {
            if (hnd_info_tab->find(ea)->_neighbour) {
                bool hnds = false;

                for (HiddenNodeDetection::NodeInfoTableIter itt = hnd_info_tab->find(ea)->_links_to.begin(); itt != hnd_info_tab->find(ea)->_links_to.end(); ++itt) {
                    if (!hnd_info_tab->find(itt.key())->_neighbour) {
                        if (!hnds) {
                            hidden_node_sa << "\t\t\t<hidden_neighbours>\n";
                            hnds = true;
                        }
                        hidden_node_sa << "\t\t\t\t<address>" << itt.key().unparse().c_str() << "</address>\n";
                    }
                }

                if (hnds) hidden_node_sa << "\t\t\t</hidden_neighbours>\n";

            }
        }
        hidden_node_sa << "\t\t</neighbour>\n";
    }

    hidden_node_sa << "\t</hiddennodes>\n";
    hidden_node_sa << "\t<hiddennodes observation_period=\"mid\">\n";


    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        hidden_node_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t count = 20;
        uint16_t temp_hidden_node = 0;
        uint16_t iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++iteration_count;
            temp_hidden_node += pls_iter->hidden_node_prob;
        }

        if (iteration_count != 0) {
            hidden_node_sa << "\t\t\t<fraction>" << temp_hidden_node / iteration_count << "</fraction>\n";
        }

        if(_cocst != NULL) {// && !_cocst->get_stats(ea_iter).empty())
            HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map;// = _cocst->get_stats(ea_iter);

            for(HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator nats_map_iter = nats_map.begin(); nats_map_iter.live(); ++nats_map_iter) {
                if(_hnd == NULL) break;

                if(*_dev->getEtherAddress() != nats_map_iter.key()
                        && !_hnd->get_nodeinfo_table()->find(nats_map_iter.key()) && !_hnd->get_nodeinfo_table()->find(nats_map_iter.key())->_neighbour) {
                    hidden_node_sa << "\t\t\t<hidden_neighbours available=\"true\">\n";
                    hidden_node_sa << "\t\t\t\t<address>" << nats_map_iter.key() << "</address>\n";
                    hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
                }
            }
        } else
        {
            hidden_node_sa << "\t\t\t<hidden_neighbours available=\"false\" />\n";
        }

        hidden_node_sa << "\t\t</neighbour>\n";
    }

    hidden_node_sa << "\t</hiddennodes>\n";
    hidden_node_sa << "\t<hiddennodes observation_period=\"long\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        hidden_node_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t count = 200;
        uint16_t temp_hidden_node = 0;
        uint16_t iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++iteration_count;
            temp_hidden_node += pls_iter->hidden_node_prob;
        }

        if(iteration_count != 0) {
            hidden_node_sa << "\t\t\t<fraction>" << temp_hidden_node / iteration_count << "</fraction>\n";
        }

        if(_cocst != NULL)// && !_cocst->get_stats(ea_iter).empty())
        {
            HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map;// = _cocst->get_stats(ea_iter);

            for(HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator nats_map_iter = nats_map.begin(); nats_map_iter.live(); ++nats_map_iter)
            {
                if(_hnd == NULL)
                {
                    click_chatter("hnd == NULL");
                    break;
                }

                if(*_dev->getEtherAddress() != nats_map_iter.key()
                        && !_hnd->get_nodeinfo_table()->find(nats_map_iter.key()) && !_hnd->get_nodeinfo_table()->find(nats_map_iter.key())->_neighbour)
                {
                    hidden_node_sa << "\t\t\t<hidden_neighbours>\n";
                    hidden_node_sa << "\t\t\t\t<address>" << nats_map_iter.key() << "</address>\n";
                    hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
                }
            }
        } else
        {
            hidden_node_sa << "\t\t\t<hidden_neighbours available=\"false\" />\n";
        }

        hidden_node_sa << "\t\t</neighbour>\n";
    }
    hidden_node_sa << "\t</hiddennodes>\n";

    return hidden_node_sa;
}

StringAccum PacketLossEstimator::stats_get_inrange(HiddenNodeDetection::NodeInfoTable *hnd_info_tab, PacketLossInformation *_pli)
{
    StringAccum inrange_sa;

    inrange_sa << "\t<inrange observation_period=\"short\">\n";

    for(int cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
        EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);

        if( ea.is_broadcast() || hnd_info_tab->find(ea) == NULL || !hnd_info_tab->find(ea)->_neighbour || (_stats_buffer.findp(ea) == NULL)) continue;

        inrange_sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
        inrange_sa << "\t\t\t<last_cw_max>" << _dev->get_cwmax()[0] << "</last_cw_max>\n";
        inrange_sa << "\t\t\t<fraction>" << _pli->graph_get(ea)->reason_get(PacketLossReason::IN_RANGE)->getFraction() << "</fraction>\n";
        inrange_sa << "\t\t</neighbour>\n";
    }

    inrange_sa << "\t</inrange>\n";
    inrange_sa << "\t<inrange observation_period=\"mid\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        inrange_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t count = 20;
        uint16_t temp_inrange = 0;
        uint16_t iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //(*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++iteration_count;
            temp_inrange += pls_iter->inrange_prob;
        }
        if (iteration_count != 0) {
            inrange_sa << "\t\t\t<fraction>" << temp_inrange / iteration_count << "</fraction>\n";
        }
        inrange_sa << "\t\t</neighbour>\n";
    }

    inrange_sa << "\t</inrange>\n";
    inrange_sa << "\t<inrange observation_period=\"long\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        inrange_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t count = 200;
        uint16_t temp_inrange = 0;
        uint16_t iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++iteration_count;
            temp_inrange += pls_iter->inrange_prob;
        }

        if(iteration_count != 0) {
            inrange_sa << "\t\t\t<fraction>" << temp_inrange / iteration_count << "</fraction>\n";
        }

        inrange_sa << "\t\t</neighbour>\n";
    }

    inrange_sa << "\t</inrange>\n";

    return inrange_sa;
}

StringAccum PacketLossEstimator::stats_get_weak_signal(HiddenNodeDetection::NodeInfoTable *hnd_info_tab, PacketLossInformation *_pli)
{
    BRN_DEBUG("StringAccum PacketLossEstimator::stats_get_weak_signal");
    StringAccum weak_signal_sa;
    weak_signal_sa << "\t<weak_signal observation_period=\"short\">\n";

    for (int cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
        EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);

        if (ea.is_broadcast() || hnd_info_tab->find(ea) == NULL || _pli->graph_get(ea) == NULL || (_stats_buffer.findp(ea) != NULL)) continue;


        weak_signal_sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
        weak_signal_sa << "\t\t\t<fraction>" << _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction() << "</fraction>\n";

        if (_cst->get_latest_stats_neighbours()->findp(ea) != NULL) {
            weak_signal_sa << "\t\t\t<last_avg_rssi>" << _cst->get_latest_stats_neighbours()->find(ea)->_avg_rssi << "</last_avg_rssi>\n";
        } else {
            weak_signal_sa << "\t\t\t<last_avg_rssi>0</last_avg_rssi>\n";
        }

        weak_signal_sa << "\t\t</neighbour>\n";
    }

    weak_signal_sa << "\t</weak_signal>\n";
    weak_signal_sa << "\t<weak_signal observation_period=\"mid\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        weak_signal_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t mid_count = 20;
        uint32_t mid_temp_weak_signal = 0;
        uint16_t mid_iteration_count = 0;

        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++mid_iteration_count;
            mid_temp_weak_signal += pls_iter->weak_signal_prob;
        }

        if (mid_iteration_count != 0) {
            weak_signal_sa << "\t\t\t<fraction>" << mid_temp_weak_signal / mid_iteration_count << "</fraction>\n";
        }

        weak_signal_sa << "\t\t</neighbour>\n";
    }

    weak_signal_sa << "\t</weak_signal>\n";
    weak_signal_sa << "\t<weak_signal observation_period=\"long\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        weak_signal_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t long_count = 200;
        uint32_t long_temp_weak_signal = 0;
        uint16_t long_iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, long_count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++long_iteration_count;
            long_temp_weak_signal += pls_iter->weak_signal_prob;
        }

        if (long_iteration_count != 0) {
            weak_signal_sa << "\t\t\t<fraction>" << long_temp_weak_signal / long_iteration_count << "</fraction>\n";
        }

        weak_signal_sa << "\t\t</neighbour>\n";
    }
    weak_signal_sa << "\t</weak_signal>\n";

    return weak_signal_sa;
}

StringAccum PacketLossEstimator::stats_get_non_wifi(HiddenNodeDetection::NodeInfoTable *hnd_info_tab, PacketLossInformation *_pli)
{
    BRN_DEBUG("StringAccum PacketLossEstimator::stats_get_non_wifi");

    StringAccum non_wifi_sa;
    non_wifi_sa << "\t<non_wifi observation_period=\"short\">\n";

    for(int cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
        EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);

        if( ea.is_broadcast() || hnd_info_tab->find(ea) == NULL || !hnd_info_tab->find(ea)->_neighbour || _pli->graph_get(ea) == NULL
                || (_stats_buffer.findp(ea) != NULL))
            continue;

        non_wifi_sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
        non_wifi_sa << "\t\t\t<fraction>" << _pli->graph_get(ea)->reason_get(PacketLossReason::NON_WIFI)->getFraction() << "</fraction>\n";
        non_wifi_sa << "\t\t</neighbour>\n";
    }

    non_wifi_sa << "\t</non_wifi>\n";
    non_wifi_sa << "\t<non_wifi observation_period=\"mid\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        non_wifi_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t count = 20;
        uint16_t temp_non_wifi = 0;
        uint16_t iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, long_count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++iteration_count;
            temp_non_wifi += pls_iter->non_wifi_prob;
        }

        if (iteration_count != 0) {
            non_wifi_sa << "\t\t\t<fraction>" << temp_non_wifi / iteration_count << "</fraction>\n";
        }
        non_wifi_sa << "\t\t</neighbour>\n";
    }

    non_wifi_sa << "\t</non_wifi>\n";
    non_wifi_sa << "\t<non_wifi observation_period=\"long\">\n";

    for (PacketLossStatisticsMapIter ea_iter = _stats_buffer.begin(); ea_iter != _stats_buffer.end(); ++ea_iter) {
        non_wifi_sa << "\t\t<neighbour address=\"" << ea_iter.key().unparse().c_str() << "\">\n";

        //uint16_t count = 200;
        uint16_t temp_non_wifi = 0;
        uint16_t iteration_count = 0;
        Vector<PacketLossStatistics> *pls = ea_iter.value(); //_stats_buffer.get_values(*ea_iter, long_count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls->begin(); pls_iter != pls->end(); ++pls_iter) {
            ++iteration_count;
            temp_non_wifi += pls_iter->non_wifi_prob;
        }

        if(iteration_count != 0) {
            non_wifi_sa << "\t\t\t<fraction>" << temp_non_wifi / iteration_count << "</fraction>\n";
        }
        non_wifi_sa << "\t\t</neighbour>\n";
    }
    non_wifi_sa << "\t</non_wifi>\n";

    return non_wifi_sa;
}

void PacketLossEstimator::update_statistics(HiddenNodeDetection::NodeInfoTable */*hnd_info_tab*/, PacketLossInformation *_pli)
{
    Timestamp now = Timestamp::now();

    for (HashMap<EtherAddress,Timestamp>::iterator ea_iter = _received_adrs.begin(); ea_iter != _received_adrs.end(); ++ea_iter) {

        if ((now.sec() - ea_iter.value().sec()) > 60) _received_adrs.remove(ea_iter.key());
        else if (now.sec() - ea_iter.value().sec() > 1) {
            //PacketParameter pm;
            _pli->graph_get(ea_iter.key())->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(100);
            _pli->graph_get(ea_iter.key())->reason_get(PacketLossReason::IN_RANGE)->setFraction(0);
            _pli->graph_get(ea_iter.key())->reason_get(PacketLossReason::NON_WIFI)->setFraction(0);
            _pli->graph_get(ea_iter.key())->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(0);

            //pm.put_params_(*ea_iter, *ea_iter, *ea_iter, 0, 0);

            //_stats_buffer.insert_values_wo_time(pm, _pli);
        }
    }
}

static String PacketLossEstimator_read_param(Element *ele, void *thunk)
{
    StringAccum sa;
    PacketLossEstimator *ple = reinterpret_cast<PacketLossEstimator *>( ele);

    switch((uintptr_t) thunk) {
      case H_STATS:
        return ple->stats_handler((uintptr_t) thunk);
        break;
    }

    return String();
}

void PacketLossEstimator::add_handlers()
{
    BRNElement::add_handlers();
    add_read_handler("stats", PacketLossEstimator_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketLossEstimator)
ELEMENT_MT_SAFE(PacketLossEstimator)
