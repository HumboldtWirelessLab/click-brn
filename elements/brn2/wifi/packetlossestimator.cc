#include "packetlossestimator.hh"

CLICK_DECLS

HashMap<EtherAddress, uint32_t> PacketLossEstimator::_acks_by_node = HashMap<EtherAddress, uint32_t> ();
StatsCircularBuffer PacketLossEstimator::_stats_buffer = StatsCircularBuffer (200);
uint8_t PacketLossEstimator::_max_weak_signal_value = 0;
HashMap<uint8_t, HashMap <uint8_t, uint64_t> > PacketLossEstimator::_rssi_histogram;

PacketLossEstimator::PacketLossEstimator () :
    _cst (NULL),
    _cinfo (NULL),
    _hnd (NULL),
    _pli (NULL),
    _cocst (NULL),
    _dev (NULL),
    _pessimistic_hn_detection (false)
{
        BRN_DEBUG ("PacketLossEstimator::PacketLossEstimator ()");
        _packet_parameter = new PacketParameter ();
}

PacketLossEstimator::~PacketLossEstimator ()
{
    BRN_DEBUG ("PacketLossEstimator::~PacketLossEstimator ()");
    delete _packet_parameter;
    _packet_parameter = NULL;
}

int PacketLossEstimator::configure (Vector<String> &conf, ErrorHandler* errh)
{
    BRN_DEBUG ("int PacketLossEstimator::configure (Vector<String> &conf, ErrorHandler* errh)");
    int ret = cp_va_kparse (conf, this, errh,
            "CHANNELSTATS", cpkP, cpElement, &_cst,
            "COLLISIONINFO", cpkP, cpElement, &_cinfo,
            "HIDDENNODE", cpkP, cpElement, &_hnd,
            "COOPCHANNELSTATS", cpkP, cpElement, &_cocst,
            "PLI", cpkP, cpElement, &_pli,
            "DEVICE", cpkP, cpElement, &_dev,
            "HNWORST", cpkP, cpBool, &_pessimistic_hn_detection,
            "DEBUG", cpkP, cpInteger, &_debug,
            cpEnd);

    return ret;
}

Packet *PacketLossEstimator::simple_action (Packet *packet)
{
    BRN_DEBUG ("Packet *PacketLossEstimator::simple_action (Packet &packet)");
    if (packet != NULL)
    {
        if (_pli != NULL)
        {
            gather_packet_infos_ (*packet);

            if (_cst != NULL)
            {
                struct airtime_stats *stats = _cst->get_latest_stats ();
                HashMap<EtherAddress, ChannelStats::SrcInfo> *src_tab = _cst->get_latest_stats_neighbours ();
                ChannelStats::SrcInfo *src_info = src_tab->findp (*_packet_parameter->get_src_address ());
                if (src_info == NULL) {
                  click_chatter("SRC not found");
                  return packet;
                }

                //click_chatter("Src:  RSSISize: %d",/*_packet_parameter->get_src_address()->unparse().c_str()*/ src_info->_rssi_hist_index);
                ChannelStats::RSSIInfo *rssi_info = _cst->get_latest_rssi_info ();
                estimateWeakSignal (src_info, *rssi_info);
                estimateNonWifi (*stats);

                /*
                BRN_DEBUG ("RX-Packets: %i\tNo-Error-Packets: %i\tRX-Retry-Packets: %i\tRX-Unicast-Packets: %i", stats->rxpackets, stats->noerr_packets, stats->rx_retry_packets, stats->rx_ucast_packets);
                BRN_DEBUG ("TX-Packets: %i\tTX-Retry-Packets: %i\tTX-Unicast-Packets: %i", stats->txpackets, stats->tx_retry_packets, stats->tx_ucast_packets);
                BRN_DEBUG ("HW-Busy: %i\tHW-RX: %i\tHW-TX: %i", stats->hw_busy, stats->hw_rx, stats->hw_tx);
                BRN_DEBUG ("HW_Cycles: %i\tHW-Busy_Cycles: %i\tHW-RX_Cycles: %i\tHW-TX_Cycles: %i", stats->hw_cycles, stats->hw_busy_cycles, stats->hw_rx_cycles, stats->hw_tx_cycles);
                */

            } else
            {
                BRN_ERROR ("Channelstats is NULL");
            }

            if (_cinfo != NULL && *_packet_parameter->get_src_address () != brn_etheraddress_broadcast)
            {
                CollisionInfo::RetryStatsTable rs_tab = _cinfo->rs_tab;
                CollisionInfo::RetryStats *rs = rs_tab.find (*_packet_parameter->get_src_address ());

                // for rs->_no_queues getfrac
                if (rs != NULL)
                {
                    BRN_INFO ("CINFO fraction for %s with queue 0: %i", _packet_parameter->get_src_address ()->unparse ().c_str (), rs->get_frac (0));
                    BRN_INFO ("CINFO fraction for %s with queue 1: %i", _packet_parameter->get_src_address ()->unparse ().c_str (), rs->get_frac (1));
                    BRN_INFO ("CINFO fraction for %s with queue 2: %i", _packet_parameter->get_src_address ()->unparse ().c_str (), rs->get_frac (2));
                    BRN_INFO ("CINFO fraction for %s with queue 3: %i", _packet_parameter->get_src_address ()->unparse ().c_str (), rs->get_frac (3));
                } else
                {
                    BRN_INFO ("CINFO fraction for %s is NULL!!!", _packet_parameter->get_src_address ()->unparse ().c_str ());
                }
            }

            if (_hnd != NULL && *_packet_parameter->get_src_address () != brn_etheraddress_broadcast)
            {
                if (_hnd->has_neighbours (*_packet_parameter->get_src_address ()))
                {
                    HiddenNodeDetection::NodeInfo*      neighbours = _hnd->get_nodeinfo_table ().find (*_packet_parameter->get_src_address ());
                    HiddenNodeDetection::NodeInfoTable  neighbours_neighbours = neighbours->get_links_to ();

                    BRN_INFO ("Neighbours of %s", _packet_parameter->get_src_address ()->unparse ().c_str ());
                } else
                {
                    BRN_INFO ("%s has no neighbours", _packet_parameter->get_src_address ()->unparse ().c_str ());
                }

                estimateHiddenNode ();
                estimateInrange ();
            }

            if (!_packet_parameter->is_broadcast_or_self ())
            {
                _stats_buffer.insert_values (*_packet_parameter, *_pli);
            }
        } else
        {
            BRN_ERROR ("PacketLossInformation is NULL");
        }
    } else
    {
        BRN_ERROR ("Packet is NULL");
    }

    return packet;
}

void PacketLossEstimator::gather_packet_infos_ (const Packet &packet)
{
    BRN_DEBUG ("void PacketLossEstimator::gather_packet_infos_ (Packet* packet)");
    struct click_wifi_extra *ceh    = WIFI_EXTRA_ANNO (&packet);
    struct click_wifi       *wh         = (struct click_wifi *) packet.data ();
    EtherAddress            own_address = *_dev->getEtherAddress ();
    EtherAddress            dst_address = EtherAddress (wh->i_addr1);
    EtherAddress            src_address;
    uint8_t                 packet_type = 0; // TODO: Enum
    StringAccum             type;
    StringAccum             subtype;

    switch (wh->i_fc[0] & WIFI_FC0_TYPE_MASK)
    {
        case WIFI_FC0_TYPE_MGT:
            type.append ("ManagementFrame");
            src_address = EtherAddress (wh->i_addr2);
            //switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {.... }
            packet_type = 10;
            break;

        case WIFI_FC0_TYPE_CTL:
            type.append ("ControlFrame");
            src_address = brn_etheraddress_broadcast;
            packet_type = 20;
            switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK)
            {
                case WIFI_FC0_SUBTYPE_PS_POLL:
                    subtype.append ("ps-poll");
                    packet_type = 21;
                    break;

                case WIFI_FC0_SUBTYPE_RTS:
                    subtype.append ("rts");
                    packet_type = 22;
                    break;

                case WIFI_FC0_SUBTYPE_CTS:
                    subtype.append ("cts");
                    packet_type = 23;
                    break;

                case WIFI_FC0_SUBTYPE_ACK:
                    subtype.append ("ack");
                    packet_type = 24;
                    add_ack (dst_address);
                    break;

                case WIFI_FC0_SUBTYPE_CF_END:
                    subtype.append ("cf-end");
                    packet_type = 25;
                    break;

                case WIFI_FC0_SUBTYPE_CF_END_ACK:
                    subtype.append ("cf-end-ack");
                    packet_type = 26;
                    break;

                default:
                    BRN_WARN ("unknown subtype: %d", (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK));
                    break;
            }
            break;

        case WIFI_FC0_TYPE_DATA:
            type.append ("DataFrame");
            src_address = EtherAddress (wh->i_addr2);
            packet_type = 30;
            break;

        default:
            BRN_WARN ("unknown type: %d", (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK));
            src_address = EtherAddress (wh->i_addr2);
            break;
    }

    if (_debug == 4)
    {
        if (type != NULL)
        {
            BRN_INFO ("Frame type: %s", type.take_string ().c_str ());
        }
        if (subtype != NULL)
        {
            BRN_INFO ("Frame subtype: %s", subtype.take_string ().c_str ());
        }
    }

    uint8_t _rate = 0;

    if (! (ceh->flags & WIFI_EXTRA_TX))
    {
        _rate = ceh->rate;
        /*BRN_INFO ("-----RATE1: %d", ceh->rate1);
        BRN_INFO ("-----RATE2: %d", ceh->rate2);
        BRN_INFO ("-----RATE3: %d", ceh->rate3);
        */
    } else
    {
        _rate = ceh->rate;
        /*BRN_INFO ("-----ELSERATE1: %d", ceh->rate1);
        BRN_INFO ("-----ELSERATE2: %d", ceh->rate2);
        BRN_INFO ("-----ELSERATE3: %d", ceh->rate3);
*/
    }

    _packet_parameter->put_params_ (own_address, src_address, dst_address, packet_type, _rate);

    if (_pli != NULL)
    {
        if (!_pli->mac_address_exists (src_address) && src_address != own_address && src_address != brn_etheraddress_broadcast)
        {
            _pli->graph_insert (src_address);
        }

        if (!_pli->mac_address_exists (dst_address) && dst_address != own_address && dst_address != brn_etheraddress_broadcast)
        {
            _pli->graph_insert (dst_address);
        }

        if (_debug == 4)
        {
            BRN_INFO ("Own Address: %s", _packet_parameter->get_own_address ()->unparse ().c_str ());
            BRN_INFO ("Source Address: %s", _packet_parameter->get_src_address ()->unparse ().c_str ());
            BRN_INFO ("Destination Address: %s", _packet_parameter->get_dst_address ()->unparse ().c_str ());
            BRN_INFO ("-----RATE: %d", _rate);
        }
    } else
    {
        BRN_ERROR ("PacketLossInformation is NULL");
    }
}

void PacketLossEstimator::estimateHiddenNode ()
{
    BRN_DEBUG ("void PacketLossEstimator::estimateHiddenNode ()");
    if (_packet_parameter->is_broadcast_or_self ())
    {
        return;
    }

    if (_hnd != NULL)
    {
        if (_packet_parameter->get_own_address () != _packet_parameter->get_dst_address ())
        {
            if (_hnd->has_neighbours (*_packet_parameter->get_own_address ()))
            {
                BRN_INFO ("Own address has neighbours");
            } else
            {
                BRN_INFO ("Own address has no neighbours");
            }

            uint8_t hnProp = 0;
            bool    coopst = false;

            if (_hnd->has_neighbours (*_packet_parameter->get_src_address ()))
            {
                BRN_INFO ("%s has neighbours", _packet_parameter->get_src_address ()->unparse ().c_str ());

                uint8_t                                 number_of_neighbours          = 0;
                HiddenNodeDetection::NodeInfo           *neighbour          = _hnd->get_nodeinfo_table ().find (*_packet_parameter->get_src_address ());
                HiddenNodeDetection::NodeInfoTable      other_neighbour_list  = neighbour->get_links_to ();
                HiddenNodeDetection::NodeInfoTableIter  other_neighbour_list_end = other_neighbour_list.end ();

                for (HiddenNodeDetection::NodeInfoTableIter i = other_neighbour_list.begin (); i != other_neighbour_list_end; i++)
                {
                    EtherAddress tmpAdd = i.key ();
                    BRN_INFO ("Neighbours Neighbour is: %s", tmpAdd.unparse ().c_str ());

                    if (!_hnd->get_nodeinfo_table ().find (tmpAdd) || !_hnd->get_nodeinfo_table ().find (tmpAdd)->_neighbour)
                    {
                        BRN_INFO ("Not in my Neighbour List");

                        if (number_of_neighbours >= 255)
                        {
                            number_of_neighbours = 255;
                        } else
                        {
                            number_of_neighbours++;
                        }
                    } else
                    {
                        BRN_INFO ("In my Neighbour List");
                    }
                }

                if (_cocst != NULL)
                {
                    EtherAddress                                            t_src = _packet_parameter->get_non_const_src_address ();
                    HashMap<EtherAddress, struct neighbour_airtime_stats*>  nats_map;

                    nats_map = _cocst->get_stats (&t_src);

                    if (!nats_map.empty ())
                    {
                        uint32_t                                                            duration = 0;
                        HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator    nats_end = nats_map.end ();

                        for (HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator i = nats_map.begin (); i != nats_end; i++)
                        {
                            EtherAddress                    tmpAdd  = i.key ();
                            struct neighbour_airtime_stats* nats    = i.value ();

                            BRN_INFO ("EA: %s", tmpAdd.unparse ().c_str ());
                            BRN_INFO ("NATS-EA: %s", EtherAddress (nats->_etheraddr).unparse ().c_str ());
                            BRN_INFO ("PKT-Count: %d", nats->_pkt_count);
                            BRN_INFO ("Duration: %d", nats->_duration);

                            if (!_hnd->get_nodeinfo_table ().find (tmpAdd) || !_hnd->get_nodeinfo_table ().find (tmpAdd)->_neighbour)
                            {
                                duration += nats->_duration;
                            }
                        }

                        if (duration != 0)
                        {
                            hnProp = 1000000/duration;
                            coopst = true;
                        }
                    } else
                    {
                        BRN_INFO ("NATS NULL");
                    }
                } else
                {
                    BRN_INFO ("_cocst == NULL");
                }

                if (!coopst)
                {
                    if (_pessimistic_hn_detection && number_of_neighbours > 0) // pessimistic estimator, if we do have one hidden node, he will send as often as possible and as slow as possible. The may interrupt all of our transmissions
                    {
                        hnProp = 100;
                    } else
                    {
                        hnProp = get_acks_by_node (*_packet_parameter->get_dst_address ()) * 12 / 10; // 12 ms per packet / 1000 ms * 100
                    }

                    BRN_INFO ("%d Acks received for %s", get_acks_by_node (*_packet_parameter->get_dst_address ()), _packet_parameter->get_dst_address ()->unparse ().c_str ());
                }

                if (_pli != NULL)
                {
                    _pli->graph_get (*_packet_parameter->get_src_address ())->reason_get (PacketLossReason::HIDDEN_NODE)->setFraction (hnProp);
                } else
                {
                    BRN_ERROR ("PacketLossInformation is NULL");
                }
                BRN_INFO (";;;;;;;;hnProp for %s: %i", _packet_parameter->get_src_address ()->unparse ().c_str (), hnProp);
            }
        }
    } else
    {
        BRN_ERROR ("HiddenNodeDetection is NULL");
    }
}

void PacketLossEstimator::estimateInrange ()
{
    BRN_DEBUG ("void PacketLossEstimator::estimateInrange ()");
    if (_packet_parameter->is_broadcast_or_self ())
    {
        return;
    }

    uint8_t                                     neighbours      = 1;
    uint8_t                                     irProp          = 0;
    HiddenNodeDetection::NodeInfoTable          neighbour_nodes = _hnd->get_nodeinfo_table ();
    HiddenNodeDetection::NodeInfoTableIter      neighbour_nodes_end = neighbour_nodes.end ();

    for (HiddenNodeDetection::NodeInfoTableIter i = neighbour_nodes.begin (); i != neighbour_nodes_end; i++)
    {
        if (neighbours >= 255)
        {
            neighbours = 255;
        } else
        {
            neighbours++;
        }
    }

    if (_dev != NULL)
    {
        uint16_t backoffsize    = _dev->get_cwmax ()[0];
        double temp             = 1.0;
        uint32_t temp_new       = 1;

        if (backoffsize == 0)
        {
            BRN_ERROR ("Can't get CWMax");
            return;
        }

        if (neighbours >= backoffsize)
        {
            irProp = 100;
        } else
        {
            for (int i = 1; i < neighbours; i++)
            {
                //click_chatter ("before temp new / temp old: %d/%d", temp, temp_new);
                temp *= (double (backoffsize) - double (i)) / double (backoffsize);
                temp_new *= ((backoffsize * 100000 - i * 100000) / (backoffsize * 100000));
                //click_chatter ("backoffsize: %d neighbour: %d", backoffsize, i);
                //click_chatter ("temp new / temp old: %d/%d", temp, temp_new);
            }

            irProp = (1 - temp) * 100;
        }

        if (_pli != NULL)
        {
            _pli->graph_get (*_packet_parameter->get_src_address ())->reason_get (PacketLossReason::IN_RANGE)->setFraction (irProp);
        } else
        {
            BRN_ERROR ("PacketLossInformation is NULL");
        }

        BRN_INFO ("Backoff/Neighbours/In-Range for %s: %d/%d/%d", _packet_parameter->get_src_address ()->unparse ().c_str (), backoffsize, neighbours, irProp);
    } else
    {
        BRN_ERROR ("BRN2Device is NULL");
    }
}

void PacketLossEstimator::estimateNonWifi (struct airtime_stats &ats)
{
    BRN_DEBUG ("void PacketLossEstimator::estimateNonWifi (struct airtime_stats *ats)");
    if (_packet_parameter->is_broadcast_or_self ())
    {
        return;
    }

    if (_pli != NULL)
    {
        /*
        if (&ats != NULL)
        {

            BRN_INFO ("Mac Busy: %d", ats.frac_mac_busy);
            BRN_INFO ("Mac RX: %d", ats.frac_mac_rx);
            BRN_INFO ("Mac TX: %d", ats.frac_mac_tx);
            BRN_INFO ("HW Busy: %d", ats.hw_busy);
            BRN_INFO ("HW RX: %d", ats.hw_rx);
            BRN_INFO ("HW TX: %d", ats.hw_tx);
            BRN_INFO ("Retries: %d", ats.rx_retry_packets);

        }
 */
        uint8_t non_wifi = 1;
        if (ats.frac_mac_busy <= ats.hw_busy)
        {
            non_wifi = ats.hw_busy - ats.frac_mac_busy;
        }

        _pli->graph_get (*_packet_parameter->get_src_address ())->reason_get (PacketLossReason::NON_WIFI)->setFraction (non_wifi);
        BRN_INFO ("hw busy/mac busy/Non-Wifi: %d/%d/%d", ats.hw_busy, ats.frac_mac_busy, non_wifi);
    } else
    {
        BRN_ERROR ("PacketLossInformation is NULL");
    }
}

void PacketLossEstimator::estimateWeakSignal (ChannelStats::SrcInfo *src_info, ChannelStats::RSSIInfo &rssi_info)
{
    BRN_DEBUG ("void PacketLossEstimator::estimateWeakSignal (ChannelStats::SrcInfo *src_info)");
    if (_packet_parameter->is_broadcast_or_self ())
    {
        BRN_INFO ("own or broadcast adresses are not suitable for weak signal estimation... continuing");
        return;
    }

    if (src_info == NULL)
    {
        BRN_ERROR ("ChannelStats::SrcInfo is NULL");
        return;
    }

    if (&rssi_info == NULL)
    {
        BRN_ERROR ("ChannelStats::RSSIInfo is NULL");
        return;
    }

    int8_t weaksignal = 100;

    if (src_info->_rssi > 0)
    {
        weaksignal = calc_weak_signal_percentage (src_info, rssi_info);
    }

    if (_hnd != NULL && _pli != NULL)
    {
        HiddenNodeDetection::NodeInfoTable hnd_info_tab = _hnd->get_nodeinfo_table ();

        for (int cou = 0; cou < _pli->node_neighbours_addresses_get ().size (); cou++)
        {
            EtherAddress ea = _pli->node_neighbours_addresses_get ().at (cou);

            if (hnd_info_tab.find (ea) != NULL && !hnd_info_tab.find (ea)->_neighbour)
            {
                _pli->graph_get (ea)->reason_get (PacketLossReason::WEAK_SIGNAL)->setFraction (100);
            } else if (ea != *_packet_parameter->get_src_address ())
            {
                int last_ws = _pli->graph_get (ea)->reason_get (PacketLossReason::WEAK_SIGNAL)->getFraction ();

                if (last_ws >= 10)
                {
                    _pli->graph_get (ea)->reason_get (PacketLossReason::WEAK_SIGNAL)->setFraction ( (last_ws + last_ws - 10) / 2);
                } else
                {
                    _pli->graph_get (ea)->reason_get (PacketLossReason::WEAK_SIGNAL)->setFraction (0);
                }
            }
        }

        _pli->graph_get (*_packet_parameter->get_src_address ())->reason_get (PacketLossReason::WEAK_SIGNAL)->setFraction (weaksignal);
        BRN_INFO ("RSSI/Weak Signal for %s: %d/%d", _packet_parameter->get_src_address ()->unparse ().c_str (), src_info->_avg_rssi, weaksignal);

    } else
    {
        BRN_ERROR ("HiddenNodeDetection and/or PacketLossInformation is NULL");
    }
}

uint8_t PacketLossEstimator::calc_weak_signal_percentage (ChannelStats::SrcInfo *src_info, ChannelStats::RSSIInfo &rssi_info)
{
    uint32_t rssi_hist_index    = src_info->_rssi_hist_index;
    uint8_t min_counter         = 0;
    uint8_t highest_counter     = 0;
    uint8_t max_counter         = 0;
    uint8_t result              = 0;
    uint8_t histogram[256];
    uint8_t min[256];
    uint8_t highest[256];
    uint8_t max[256];
    uint8_t min_val[256];
    uint8_t highest_val[256];
    uint8_t max_val[256];

    memset (histogram, 0, sizeof (histogram));
    memset (min, 0, sizeof (min));
    memset (highest, 0, sizeof (highest));
    memset (max, 0, sizeof (max));
    memset (min_val, 0, sizeof (min_val));
    memset (highest_val, 0, sizeof (highest_val));
    memset (max_val, 0, sizeof (max_val));

    if (&rssi_info == NULL)
    {
        BRN_INFO ("rssi_info NULL");
    }

    /*
     * go through the history and create a real histogram, because we get only 50 values in any order
     */

    for (uint8_t i = 0; i < rssi_hist_index; i++)
    {
        //BRN_INFO ("histogramm_val: %d", src_info->_rssi_hist[i]);
        if (src_info->_rssi_hist[i] == 0)
        {
            continue;
        } else
        {
            histogram[src_info->_rssi_hist[i]]++;
        }
    }

    /*
     * find min, max and hightest values
     */

    min[min_counter] = highest[highest_counter] = max[max_counter] = histogram[0];
    //BRN_INFO ("histogramm START");
    for (uint16_t j = 0; j < 256; j++)
    {
        //BRN_INFO ("histogramm %d: %d", j, histogram[j]);
        if (min[min_counter] < histogram[j])
        {
            if (min_counter <= highest_counter)
            {
                ++min_counter;
            }
        } else
        {
            if (j < 255)
            {
                min[min_counter] = histogram[j];
                min_val[min_counter] = j;
            } else {
                min_val[min_counter] = 0;
            }
        }

        if (highest[highest_counter] < histogram[j])
        {
            highest[highest_counter] = histogram[j];
            max[highest_counter] = histogram[j];
            highest_val[highest_counter] = j;
            max_val[highest_counter] = j;

        } else
        {
            if (min_counter > highest_counter)
            {
                highest_counter = min_counter;
            }
        }

        if (max[max_counter] > histogram[j] && highest[max_counter] != 0 && max[max_counter])
        {
            max[max_counter] = histogram[j];
            max_val[max_counter] = j;
        } else
        {
            if (max_counter < highest_counter)
            {
                max_counter = highest_counter;
            }
        }
    }
    //BRN_INFO ("histogramm END");

    BRN_INFO ("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[0], highest_val[0], max_val[0]);
    BRN_INFO ("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[1], highest_val[1], max_val[1]);
    BRN_INFO ("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[2], highest_val[2], max_val[2]);
    BRN_INFO ("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[3], highest_val[3], max_val[3]);
    BRN_INFO ("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[4], highest_val[4], max_val[4]);
    BRN_INFO ("histogramm MIN, HIGHEST, MAX 0: %d/%d/%d", min_val[5], highest_val[5], max_val[5]);

    uint8_t highval = max_val[0] - highest_val[0];

    if (highest_val[0] - highval < 1)
    {
        if ((highval - highest_val[0] - min_val[0]) * 2 < highval)
        {
            result = 100;
        } else
        {
            result = 50;
        }
    } else
    {
        result = 0;
    }

    return result;
}

inline void PacketLossEstimator::add_ack (const EtherAddress &dest_addr)
{
    BRN_DEBUG ("void PacketLossEstimator::add_ack (EtherAddress dest_addr)");
    uint32_t acks = PacketLossEstimator::_acks_by_node.find (dest_addr);
    PacketLossEstimator::_acks_by_node.erase (dest_addr);
    PacketLossEstimator::_acks_by_node.insert (dest_addr, ++acks);
}

inline uint32_t PacketLossEstimator::get_acks_by_node (const EtherAddress &dest_addr)
{
    BRN_DEBUG ("uint32_t PacketLossEstimator::get_acks_by_node (EtherAddress dest_addr)");
    return PacketLossEstimator::_acks_by_node.find (dest_addr);
}

inline void PacketLossEstimator::reset_acks ()
{
    for (HashMap<EtherAddress, uint32_t>::iterator i = PacketLossEstimator::_acks_by_node.begin(); i != PacketLossEstimator::_acks_by_node.end (); i++)
    {
        i.value() = 0;
    }
}

////////////////////////// STATS //////////////////////////////

enum
{
    H_STATS
};

String PacketLossEstimator::stats_handler (int /*mode*/)
{
    if (_debug == 4)
    {
        click_chatter ("String PacketLossEstimator::stats_handler");
    }
    StringAccum sa;

    sa << "<packetlossreason" << " node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now () << "\">\n";
    if (_pli != NULL)
    {
        if (_hnd != NULL)
        {
            HiddenNodeDetection::NodeInfoTable hnd_info_tab = _hnd->get_nodeinfo_table ();

            sa << stats_get_hidden_node (hnd_info_tab, *_pli);
            sa << stats_get_inrange (hnd_info_tab, *_pli);
            sa << stats_get_weak_signal (hnd_info_tab, *_pli);
            sa << stats_get_non_wifi (hnd_info_tab, *_pli);
        }
    }

    sa << "</packetlossreason>\n";
    return sa.take_string ();
}

StringAccum PacketLossEstimator::stats_get_hidden_node (HiddenNodeDetection::NodeInfoTable &hnd_info_tab, PacketLossInformation &_pli)
{
    if (_debug == 4)
    {
        click_chatter ("StringAccum PacketLossEstimator::stats_get_hidden_node");
    }

    reset_acks();
    StringAccum     hidden_node_sa;
    const int       neighbour_no = hnd_info_tab.size ();
    EtherAddress    neighbours[neighbour_no];
    int             cou = 0;

    for (HiddenNodeDetection::NodeInfoTableIter i = hnd_info_tab.begin (); i != hnd_info_tab.end (); i++)
    {
        if (cou >= neighbour_no)
        {
            break;
        }

        neighbours[cou++] = i.key ();
    }

    hidden_node_sa << "\t<hiddennodes observation_period=\"short\">\n";

    for (cou = 0; cou < _pli.node_neighbours_addresses_get ().size (); cou++)
    {
        EtherAddress ea = _pli.node_neighbours_addresses_get ().at (cou);

        if (ea == brn_etheraddress_broadcast || hnd_info_tab.find (ea) == NULL || !hnd_info_tab.find (ea)->_neighbour)
        {
            continue;
        }

        hidden_node_sa  << "\t\t<neighbour address=\"" << ea.unparse ().c_str () << "\">\n";
        hidden_node_sa  << "\t\t\t<fraction>"
                        << _pli.graph_get (ea)->reason_get (PacketLossReason::HIDDEN_NODE)->getFraction ()
                        << "</fraction>\n";

        if (_cocst != NULL)
        {
            HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map = _cocst->get_stats (&ea);

            for (HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator nats_map_iter = nats_map.begin (); nats_map_iter.live (); ++nats_map_iter)
            {

                const EtherAddress ea = nats_map_iter.key ();

                if (*_dev->getEtherAddress() == ea)
                {
                    click_chatter("address: %s", ea.unparse().c_str());
                    continue;
                } else if (_hnd == NULL)
                {
                    click_chatter("hnd == NULL");
                    break;
                }

                if (_hnd->get_nodeinfo_table ().find (ea) != NULL && _hnd->get_nodeinfo_table ().find (ea)->_neighbour)
                {
                    click_chatter("address: %s, neighbour = true", ea.unparse().c_str());
                    continue;
                }

                hidden_node_sa << "\t\t\t<hidden_neighbours available=\"true\">\n";
                hidden_node_sa << "\t\t\t\t<address>" << ea.unparse().c_str() << "</address>\n";
                hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
            }
        } else
        {
            if (hnd_info_tab.find (ea)->_neighbour)
            {
                bool hnds = false;

                for (HiddenNodeDetection::NodeInfoTableIter itt = hnd_info_tab.find (ea)->_links_to.begin ();
                     itt != hnd_info_tab.find (ea)->_links_to.end ();
                     itt++)
                {
                    if (!hnd_info_tab.find (itt.key ())->_neighbour)
                    {
                        if (!hnds)
                        {
                            hidden_node_sa << "\t\t\t<hidden_neighbours>\n";
                            hnds = true;
                        }
                        hidden_node_sa << "\t\t\t\t<address>" << itt.key ().unparse ().c_str () << "</address>\n";
                    }
                }
                if (hnds)
                {
                    hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
                }
            }
        }
        hidden_node_sa << "\t\t</neighbour>\n";
    }

    hidden_node_sa << "\t</hiddennodes>\n";
    hidden_node_sa << "\t<hiddennodes observation_period=\"mid\">\n";

    Vector<EtherAddress> etheraddresses = _stats_buffer.get_stored_addresses ();

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        hidden_node_sa << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        count = 20;
        uint16_t                        temp_hidden_node = 0;
        uint16_t                        iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++iteration_count;
            temp_hidden_node += pls_iter->get_hidden_node_probability ();
        }

        if (iteration_count != 0)
        {
            click_chatter ("hiddennode_sum/iter_count: %d/%d", temp_hidden_node , iteration_count);
            hidden_node_sa  << "\t\t\t<fraction>" << temp_hidden_node / iteration_count << "</fraction>\n";
        }

        if (_cocst != NULL)
        {
            HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map = _cocst->get_stats (ea_iter);

            for (HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator nats_map_iter = nats_map.begin (); nats_map_iter.live (); ++nats_map_iter)
            {
                if (_hnd == NULL)
                {
                    click_chatter("hnd == NULL");
                    break;
                }
                if (*_dev->getEtherAddress() != nats_map_iter.key () && !_hnd->get_nodeinfo_table ().find (nats_map_iter.key ()) /*&& !_hnd->get_nodeinfo_table ().find (nats_map_iter.key ())->_neighbour*/)
                {
                    hidden_node_sa << "\t\t\t<hidden_neighbours available=\"true\">\n";
                    hidden_node_sa << "\t\t\t\t<address>" << nats_map_iter.key () << "</address>\n";
                    hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
                }
        	}
        } else
        {
            hidden_node_sa << "\t\t\t<hidden_neighbours available=\"false\" \\>\n";
        }

        hidden_node_sa << "\t\t</neighbour>\n";
    }

    hidden_node_sa << "\t</hiddennodes>\n";
    hidden_node_sa << "\t<hiddennodes observation_period=\"long\">\n";

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        hidden_node_sa  << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        count = 200;
        uint16_t                        temp_hidden_node = 0;
        uint16_t                        iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++iteration_count;
            temp_hidden_node += pls_iter->get_hidden_node_probability ();
        }

        if (iteration_count != 0)
        {
            hidden_node_sa << "\t\t\t<fraction>" << temp_hidden_node / iteration_count << "</fraction>\n";
        }

        if (_cocst != NULL)
        {
            HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map = _cocst->get_stats (ea_iter);

            for (HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator nats_map_iter = nats_map.begin (); nats_map_iter.live (); ++nats_map_iter)
            {
                if (_hnd == NULL)
                {
                    click_chatter("hnd == NULL");
                    break;
                }

                if (*_dev->getEtherAddress() != nats_map_iter.key () && !_hnd->get_nodeinfo_table ().find (nats_map_iter.key ()) /*&& !_hnd->get_nodeinfo_table ().find (nats_map_iter.key ())->_neighbour*/)
                {
                    hidden_node_sa << "\t\t\t<hidden_neighbours>\n";
                    hidden_node_sa << "\t\t\t\t<address>" << nats_map_iter.key () << "</address>\n";
                    hidden_node_sa << "\t\t\t</hidden_neighbours>\n";
                }
            }
        } else
        {
            click_chatter ("hiddennode_sum/iter_count: %d/%d", temp_hidden_node , iteration_count);
            hidden_node_sa << "\t\t\t<hidden_neighbours available=\"false\" \\>\n";
        }

        hidden_node_sa << "\t\t</neighbour>\n";
    }
    hidden_node_sa << "\t</hiddennodes>\n";

    return hidden_node_sa;
}

StringAccum PacketLossEstimator::stats_get_inrange (HiddenNodeDetection::NodeInfoTable &hnd_info_tab, PacketLossInformation &_pli)
{
    StringAccum inrange_sa;

    inrange_sa << "\t<inrange observation_period=\"short\">\n";

    for (int cou = 0; cou < _pli.node_neighbours_addresses_get ().size (); cou++)
    {
        EtherAddress ea = _pli.node_neighbours_addresses_get ().at (cou);

        if (ea == brn_etheraddress_broadcast || hnd_info_tab.find (ea) == NULL || !hnd_info_tab.find (ea)->_neighbour)
        {
            continue;
        }

        inrange_sa  << "\t\t<neighbour address=\"" << ea.unparse ().c_str () << "\">\n";
        inrange_sa  << "\t\t\t<fraction>"
                    << _pli.graph_get (ea)->reason_get (PacketLossReason::IN_RANGE)->getFraction ()
                    << "</fraction>\n";
        inrange_sa  << "\t\t</neighbour>\n";
    }

    inrange_sa << "\t</inrange>\n";
    inrange_sa << "\t<inrange observation_period=\"mid\">\n";

    Vector<EtherAddress> etheraddresses = _stats_buffer.get_stored_addresses ();

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        inrange_sa  << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        count = 20;
        uint16_t                        temp_inrange = 0;
        uint16_t                        iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++iteration_count;
            temp_inrange += pls_iter->get_inrange_probability ();
        }
        if (iteration_count != 0)
        {
            inrange_sa << "\t\t\t<fraction>" << temp_inrange / iteration_count << "</fraction>\n";
        }
        inrange_sa << "\t\t</neighbour>\n";
    }

    inrange_sa << "\t</inrange>\n";
    inrange_sa << "\t<inrange observation_period=\"long\">\n";

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        inrange_sa  << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        count = 200;
        uint16_t                        temp_inrange = 0;
        uint16_t                        iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++iteration_count;
            temp_inrange += pls_iter->get_inrange_probability ();
        }
        if (iteration_count != 0)
        {
            inrange_sa << "\t\t\t<fraction>" << temp_inrange / iteration_count << "</fraction>\n";
        }

        inrange_sa << "\t\t</neighbour>\n";
    }

    inrange_sa << "\t</inrange>\n";

    return inrange_sa;
}

StringAccum PacketLossEstimator::stats_get_weak_signal (HiddenNodeDetection::NodeInfoTable &hnd_info_tab, PacketLossInformation &_pli)
{
    BRN_DEBUG ("StringAccum PacketLossEstimator::stats_get_weak_signal");
    StringAccum weak_signal_sa;
    weak_signal_sa << "\t<weak_signal observation_period=\"short\">\n";

    for (int cou = 0; cou < _pli.node_neighbours_addresses_get ().size (); cou++)
    {
        EtherAddress ea = _pli.node_neighbours_addresses_get ().at (cou);

        if (ea == brn_etheraddress_broadcast || hnd_info_tab.find (ea) == NULL)
        {
            continue;
        }
        weak_signal_sa  << "\t\t<neighbour address=\"" << ea.unparse ().c_str () << "\">\n";
        weak_signal_sa  << "\t\t\t<fraction>"
                        << _pli.graph_get (ea)->reason_get (PacketLossReason::WEAK_SIGNAL)->getFraction ()
                        << "</fraction>\n";
        weak_signal_sa  << "\t\t</neighbour>\n";
    }

    weak_signal_sa << "\t</weak_signal>\n";
    weak_signal_sa << "\t<weak_signal observation_period=\"mid\">\n";

    Vector<EtherAddress> etheraddresses = _stats_buffer.get_stored_addresses ();

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        weak_signal_sa << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        mid_count = 20;
        uint32_t                        mid_temp_weak_signal = 0;
        uint16_t                        mid_iteration_count = 0;

        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, mid_count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++mid_iteration_count;
            mid_temp_weak_signal += pls_iter->get_weak_signal_probability ();
        }

        if (mid_iteration_count != 0)
        {
            weak_signal_sa << "\t\t\t<fraction>" << mid_temp_weak_signal / mid_iteration_count << "</fraction>\n";
        }
        weak_signal_sa << "\t\t</neighbour>\n";
    }

    weak_signal_sa << "\t</weak_signal>\n";
    weak_signal_sa << "\t<weak_signal observation_period=\"long\">\n";

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        weak_signal_sa  << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        long_count = 200;
        uint32_t                        long_temp_weak_signal= 0;
        uint16_t                        long_iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, long_count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); ++pls_iter)
        {
            ++long_iteration_count;
            long_temp_weak_signal += pls_iter->get_weak_signal_probability ();
        }

        if (long_iteration_count != 0)
        {
            weak_signal_sa << "\t\t\t<fraction>" << long_temp_weak_signal / long_iteration_count << "</fraction>\n";
        }
        weak_signal_sa << "\t\t</neighbour>\n";
    }
    weak_signal_sa << "\t</weak_signal>\n";

    return weak_signal_sa;
}

StringAccum PacketLossEstimator::stats_get_non_wifi (HiddenNodeDetection::NodeInfoTable &hnd_info_tab, PacketLossInformation &_pli)
{
    BRN_DEBUG ("StringAccum PacketLossEstimator::stats_get_non_wifi");

    StringAccum non_wifi_sa;
    non_wifi_sa << "\t<non_wifi observation_period=\"short\">\n";

    for (int cou = 0; cou < _pli.node_neighbours_addresses_get ().size (); cou++)
    {
        EtherAddress ea = _pli.node_neighbours_addresses_get ().at (cou);

        if (ea == brn_etheraddress_broadcast || hnd_info_tab.find (ea) == NULL || !hnd_info_tab.find (ea)->_neighbour)
        {
            continue;
        }
        non_wifi_sa << "\t\t<neighbour address=\"" << ea.unparse ().c_str () << "\">\n";
        non_wifi_sa << "\t\t\t<fraction>"
                    << _pli.graph_get (ea)->reason_get (PacketLossReason::NON_WIFI)->getFraction ()
                    << "</fraction>\n";
        non_wifi_sa << "\t\t</neighbour>\n";
    }

    non_wifi_sa << "\t</non_wifi>\n";
    non_wifi_sa << "\t<non_wifi observation_period=\"mid\">\n";

    Vector<EtherAddress> etheraddresses = _stats_buffer.get_stored_addresses ();

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        non_wifi_sa  << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        count = 20;
        uint16_t                        temp_non_wifi = 0;
        uint16_t                        iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++iteration_count;
            temp_non_wifi += pls_iter->get_non_wifi_probability ();
        }
        if (iteration_count != 0)
        {
            non_wifi_sa << "\t\t\t<fraction>" << temp_non_wifi / iteration_count << "</fraction>\n";
        }
        non_wifi_sa << "\t\t</neighbour>\n";
    }

    non_wifi_sa << "\t</non_wifi>\n";
    non_wifi_sa << "\t<non_wifi observation_period=\"long\">\n";

    for (Vector<EtherAddress>::iterator ea_iter = etheraddresses.begin (); ea_iter != etheraddresses.end (); ea_iter++)
    {
        non_wifi_sa  << "\t\t<neighbour address=\"" << ea_iter->unparse ().c_str () << "\">\n";

        uint16_t                        count = 200;
        uint16_t                        temp_non_wifi = 0;
        uint16_t                        iteration_count = 0;
        Vector<PacketLossStatistics>    pls = _stats_buffer.get_values (*ea_iter, count);

        for (Vector<PacketLossStatistics>::iterator pls_iter = pls.begin (); pls_iter != pls.end (); pls_iter++)
        {
            ++iteration_count;
            temp_non_wifi += pls_iter->get_non_wifi_probability ();
        }
        if (iteration_count != 0)
        {
            non_wifi_sa << "\t\t\t<fraction>" << temp_non_wifi / iteration_count << "</fraction>\n";
        }
        non_wifi_sa << "\t\t</neighbour>\n";
    }
    non_wifi_sa << "\t</non_wifi>\n";

    return non_wifi_sa;
}

static String PacketLossEstimator_read_param (Element *ele, void *thunk)
{
    StringAccum         sa;
    PacketLossEstimator *ple = (PacketLossEstimator *) ele;

    switch ( (uintptr_t) thunk)
    {
        case H_STATS:
            return ple->stats_handler ( (uintptr_t) thunk);
            break;
    }

    return String ();
}

void PacketLossEstimator::add_handlers ()
{
    BRNElement::add_handlers ();
    add_read_handler ("stats", PacketLossEstimator_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(PacketParameter)
ELEMENT_REQUIRES(StatsCircularBuffer)
EXPORT_ELEMENT(PacketLossEstimator)
ELEMENT_MT_SAFE(PacketLossEstimator)
