#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <math.h>

#if CLICK_NS 
        #include <click/router.hh>
#endif
#include "packetlossestimator.hh"
#include "elements/brn2/wifi/collisioninfo.hh"

CLICK_DECLS

PacketLossEstimator::PacketLossEstimator() :
    _cst(NULL),
    _cinfo(NULL),
    _hnd(NULL),
    _pli(NULL),
    _midterm(5),
    _longterm(10),
    _dev(NULL) {
        
        BRN_DEBUG("PacketLossEstimator::PacketLossEstimator()");
        _packet_parameter = new PacketParameter();
}

PacketLossEstimator::~PacketLossEstimator() {

    BRN_DEBUG("PacketLossEstimator::~PacketLossEstimator()");
    delete _packet_parameter;
}

HashMap<EtherAddress, uint32_t> PacketLossEstimator::_acks_by_node = HashMap<EtherAddress, uint32_t>();
PacketLossEstimator::StatsCircularBuffer stats_buffer = PacketLossEstimator::StatsCircularBuffer(200);

int PacketLossEstimator::configure(Vector<String> &conf, ErrorHandler* errh) {
    
    BRN_DEBUG("int PacketLossEstimator::configure(Vector<String> &conf, ErrorHandler* errh)");
    int ret = cp_va_kparse(conf, this, errh,
            "CHANNELSTATS", cpkP, cpElement, &_cst,
            "COLLISIONINFO", cpkP, cpElement, &_cinfo,
            "HIDDENNODE", cpkP, cpElement, &_hnd,
            "COOPCHANNELSTATS", cpkP, cpElement, &_cocst,
            "PLI", cpkP, cpElement, &_pli,
            "DEVICE", cpkP, cpElement, &_dev,
            "HNWORST", cpkP, cpBool, &_pessimistic_hn_detection,
            "MIDTERMINTERVAL", cpkP, cpInteger, &_midterm,
            "LONGTERMINTERVAL", cpkP, cpInteger, &_longterm,
            "DEBUG", cpkP, cpInteger, &_debug,
            cpEnd);
    
    return ret;
}

Packet *PacketLossEstimator::simple_action(Packet *packet) {
    
    BRN_DEBUG("Packet *PacketLossEstimator::simple_action(Packet *packet)");
    if (packet != NULL) {
        
        if (_pli != NULL) {
            
            gather_packet_infos_(packet);
            struct airtime_stats *stats;
            HashMap<EtherAddress, ChannelStats::SrcInfo> *src_tab;
            HashMap<EtherAddress, airtime_stats> ether_stats;
            
            if (_cst != NULL) {
                
                int time_now = packet->timestamp_anno().msec();
                
                if (_packet_parameter->get_src_address() != brn_etheraddress_broadcast && _packet_parameter->get_src_address() != _packet_parameter->get_own_address()) {
                    
                    click_chatter("TIME: %d", time_now);
                    
                    ether_stats.insert(_packet_parameter->get_src_address(), *_cst->get_latest_stats());
                    
                    stats_buffer.put_data_in(&ether_stats);
                    
                    
/*   TO DELETE                 click_chatter("%s", _packet_parameter->get_src_address().unparse().c_str());
                    
                    if (!_mid_term_pli.mac_address_exists(_packet_parameter->get_src_address())) {
                        
                        click_chatter("midterm %s does not exist!!!!", _packet_parameter->get_src_address().unparse().c_str());
                        _mid_term_pli.graph_insert(_packet_parameter->get_src_address());
                    }
                    
                    if (!_long_term_pli.mac_address_exists(_packet_parameter->get_src_address())) {
                        
                        click_chatter("longterm %s does not exist!!!!", _packet_parameter->get_src_address().unparse().c_str());
                        _long_term_pli.graph_insert(_packet_parameter->get_src_address());
                    }  
                    
                    if (_mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS) == NULL)
                        _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS)->setFraction(packet->timestamp_anno().msec() * 1000);
                    
                    else if (time_now - (_mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS)->getFraction()) > _midterm) {
                        
                        _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS)->setFraction(packet->timestamp_anno().msec() * 1000);
                        _ple_interv.mid_now = true;
                    }
                    
                    if (_long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS) == NULL)
                        _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS)->setFraction(packet->timestamp_anno().msec() * 1000);
                    
                    else if (time_now - (_long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS)->getFraction()) > _longterm) {
                        
                        _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::PACKET_LOSS)->setFraction(packet->timestamp_anno().msec() * 1000);
                        _ple_interv.long_now = true;
                    }*/
                }
                
                stats = _cst->get_latest_stats();
                src_tab = _cst->get_latest_stats_neighbours();
                ChannelStats::SrcInfo srcInfo = src_tab->find(_packet_parameter->get_src_address());
                estimateWeakSignal(&srcInfo);
                estimateNonWifi(stats);
            }
            
            if (_cinfo != NULL && _packet_parameter->get_src_address() != brn_etheraddress_broadcast) {
                
                CollisionInfo::RetryStatsTable rs_tab = _cinfo->rs_tab;
                CollisionInfo::RetryStats *rs = rs_tab.find(_packet_parameter->get_src_address());
                
                // for rs->_no_queues getfrac
                if (rs != NULL) {
                    
                    BRN_DEBUG("CINFO fraction for %s with queue 0: %i", _packet_parameter->get_src_address().unparse().c_str(), rs->get_frac(0));
                    BRN_DEBUG("CINFO fraction for %s with queue 1: %i", _packet_parameter->get_src_address().unparse().c_str(), rs->get_frac(1));
                    BRN_DEBUG("CINFO fraction for %s with queue 2: %i", _packet_parameter->get_src_address().unparse().c_str(), rs->get_frac(2));
                    BRN_DEBUG("CINFO fraction for %s with queue 3: %i", _packet_parameter->get_src_address().unparse().c_str(), rs->get_frac(3));
                
                } else
                    BRN_DEBUG("CINFO fraction for %s is NULL!!!", _packet_parameter->get_src_address().unparse().c_str());
            }
            
            if (_hnd != NULL && _packet_parameter->get_src_address() != brn_etheraddress_broadcast) {
                
                if (_hnd->has_neighbours(_packet_parameter->get_src_address())) {
                    
                    HiddenNodeDetection::NodeInfo* neighbours = _hnd->get_nodeinfo_table().find(_packet_parameter->get_src_address());
                    HiddenNodeDetection::NodeInfoTable neighbours_neighbours = neighbours->get_links_to();
                    BRN_DEBUG("Neighbours of %s", _packet_parameter->get_src_address().unparse().c_str());
                    
                    for (HiddenNodeDetection::NodeInfoTableIter i = neighbours_neighbours.begin(); i != neighbours_neighbours.end(); i++) {
                        
                        BRN_DEBUG("  Neighbour: %s", i.key().unparse().c_str());
                    }
                    
                } else
                    BRN_DEBUG("%s has no neighbours", _packet_parameter->get_src_address().unparse().c_str());
                
                estimateHiddenNode();
                estimateInrange();
            }

            if (_cst != NULL) {
                /*
                BRN_DEBUG("RX-Packets: %i\tNo-Error-Packets: %i\tRX-Retry-Packets: %i\tRX-Unicast-Packets: %i", stats->rxpackets, stats->noerr_packets, stats->rx_retry_packets, stats->rx_ucast_packets);
                BRN_DEBUG("TX-Packets: %i\tTX-Retry-Packets: %i\tTX-Unicast-Packets: %i", stats->txpackets, stats->tx_retry_packets, stats->tx_ucast_packets);
                BRN_DEBUG("HW-Busy: %i\tHW-RX: %i\tHW-TX: %i", stats->hw_busy, stats->hw_rx, stats->hw_tx);
                BRN_DEBUG("HW_Cycles: %i\tHW-Busy_Cycles: %i\tHW-RX_Cycles: %i\tHW-TX_Cycles: %i", stats->hw_cycles, stats->hw_busy_cycles, stats->hw_rx_cycles, stats->hw_tx_cycles);
            */
            }
        }
        
        _ple_interv.mid_now = false;
        _ple_interv.long_now = false;

        return packet;
    }
}

void PacketLossEstimator::gather_packet_infos_(Packet* packet) {

    BRN_DEBUG("void PacketLossEstimator::gather_packet_infos_(Packet* packet)");
    struct click_wifi *wh = (struct click_wifi *) packet->data();
    EtherAddress own_address = *_dev->getEtherAddress();
    EtherAddress dst_address = EtherAddress(wh->i_addr1);
    EtherAddress src_address;
    uint8_t packet_type = 0;
    StringAccum type;
    StringAccum subtype;
    
    switch (wh->i_fc[0] & WIFI_FC0_TYPE_MASK) {

        case WIFI_FC0_TYPE_MGT:
            type.append("ManagementFrame");
            src_address = EtherAddress(wh->i_addr2);
            //switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {.... }
            packet_type = 10;
            break;

        case WIFI_FC0_TYPE_CTL:
            type.append("ControlFrame");
            src_address = brn_etheraddress_broadcast;
            packet_type = 20;
            switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {

                case WIFI_FC0_SUBTYPE_PS_POLL:
                    subtype.append("ps-poll");
                    packet_type = 21;
                    break;
                    
                case WIFI_FC0_SUBTYPE_RTS:
                    subtype.append("rts");
                    packet_type = 22;
                    break;
                    
                case WIFI_FC0_SUBTYPE_CTS:
                    subtype.append("cts");
                    packet_type = 23;
                    break;
                    
                case WIFI_FC0_SUBTYPE_ACK:
                    subtype.append("ack");
                    packet_type = 24;
                    add_ack(dst_address);
                    break;
                    
                case WIFI_FC0_SUBTYPE_CF_END:
                    subtype.append("cf-end");
                    packet_type = 25;
                    break;
                    
                case WIFI_FC0_SUBTYPE_CF_END_ACK:
                    subtype.append("cf-end-ack");
                    packet_type = 26;
                    break;
                    
                default:
                    BRN_WARN("unknown subtype: %d", (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK));
            }
            break;

        case WIFI_FC0_TYPE_DATA:
            type.append("DataFrame");
            src_address = EtherAddress(wh->i_addr2);
            packet_type = 30;
            break;

        default:
            BRN_WARN("unknown type: %d", (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK));
            src_address = EtherAddress(wh->i_addr2);
    }
    
    if (_debug == 4) {
        
        if (type != NULL)
            BRN_INFO("Frame type: %s", type.take_string().c_str());
            
        if (subtype != NULL)
            BRN_INFO("Frame subtype: %s", subtype.take_string().c_str());
    }
    
    _packet_parameter->put_params_(own_address, src_address, dst_address, packet_type);
    
    if (!_pli->mac_address_exists(src_address) && src_address != own_address && src_address != brn_etheraddress_broadcast) {
        
        _pli->graph_insert(src_address);
        _mid_term_pli.graph_insert(src_address);
        _long_term_pli.graph_insert(src_address);
    }
    
    if (!_pli->mac_address_exists(dst_address) && dst_address != own_address && dst_address != brn_etheraddress_broadcast) {
        
        _pli->graph_insert(dst_address);
        _mid_term_pli.graph_insert(dst_address);
        _long_term_pli.graph_insert(dst_address);
    }
    
    if (_debug == 4) {
        
        BRN_INFO("Own Address: %s", _packet_parameter->get_own_address().unparse().c_str());
        BRN_INFO("Source Address: %s", _packet_parameter->get_src_address().unparse().c_str());
        BRN_INFO("Destination Address: %s", _packet_parameter->get_dst_address().unparse().c_str());
    }
}

void PacketLossEstimator::estimateHiddenNode() {

    BRN_DEBUG("void PacketLossEstimator::estimateHiddenNode()");
    if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
        return;
    
    if (_packet_parameter->get_own_address() != _packet_parameter->get_dst_address()) {
        
        if (_hnd->has_neighbours(_packet_parameter->get_own_address()))
            BRN_INFO("Own address has neighbours");
        else
            BRN_INFO("Own address has no neighbours");

        uint8_t hnProp = 6;
        bool coopst = false;
        
        if (_hnd->has_neighbours(_packet_parameter->get_src_address())) {
            
            BRN_INFO("%s has neighbours", _packet_parameter->get_src_address().unparse().c_str());
            
            HiddenNodeDetection::NodeInfo *neighbour = _hnd->get_nodeinfo_table().find(_packet_parameter->get_src_address());
            HiddenNodeDetection::NodeInfoTable otherNeighbourList = neighbour->get_links_to();
            
            uint8_t neighbours = 0;

            for (HiddenNodeDetection::NodeInfoTableIter i = otherNeighbourList.begin(); i != otherNeighbourList.end(); i++) {
                
                EtherAddress tmpAdd = i.key();
                BRN_INFO("Neighbours Neighbour is: %s", tmpAdd.unparse().c_str());

                if (!_hnd->get_nodeinfo_table().find(tmpAdd) || !_hnd->get_nodeinfo_table().find(tmpAdd)->_neighbour) {

                    BRN_INFO("Not in my Neighbour List");
                    //uint32_t silence_period = Timestamp::now().msec() - _hnd->get_nodeinfo_table().find(_packet_parameter->get_src_address())->get_links_usage().find(tmpAdd).msec();

                    if (neighbours >= 255)
                        neighbours = 255;
                    else
                        neighbours++;

                } else
                    BRN_INFO("In my Neighbour List");
            }
            
            if (_cocst != NULL) {
                
                HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map = _cocst->get_stats(&_packet_parameter->get_src_address());
                
                if (!nats_map.empty()) {
                    
                    BRN_INFO("NATS-size: %d", nats_map.size());
                    
                    uint8_t duration = 0;
                    
                    for (HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator i = nats_map.begin(); i != nats_map.end(); i++) {
                        
                        EtherAddress tmpAdd = i.key();
                        struct neighbour_airtime_stats* nats = i.value();
                        
                        BRN_INFO("EA: %s", tmpAdd.unparse().c_str());
                        BRN_INFO("NATS-EA: %s", EtherAddress(nats->_etheraddr).unparse().c_str());
                        BRN_INFO("PKT-Count: %d", nats->_pkt_count);
                        BRN_INFO("Duration: %d", nats->_duration);
                        
                        if (!_hnd->get_nodeinfo_table().find(tmpAdd) || !_hnd->get_nodeinfo_table().find(tmpAdd)->_neighbour)
                            duration += nats->_duration;
                    }
                    
                    if (duration != 0) {
                        
                        hnProp = 1000000/duration;
                        coopst = true;
                    }
                    
                } else
                    BRN_INFO("NATS NULL");
            }
            
            if (!coopst) {
                
                if (_pessimistic_hn_detection && neighbours > 0) { // pessimistic estimator, if we do have one hidden node, he will send as often as possible and as slow as possible. The may interrupt all of our transmissions
                
                    hnProp = 100;
                    
                } else {
                    
                    //TODO: clear stats after 1 sec
                    hnProp = get_acks_by_node(_packet_parameter->get_dst_address()) * 12 /10; // 12 ms per packet / 1000 ms * 100
                }
                
                BRN_INFO("%d Acks received for %s", get_acks_by_node(_packet_parameter->get_dst_address()), _packet_parameter->get_dst_address().unparse().c_str());
            }
            
            int last_hn = _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction();
            hnProp = (last_hn + hnProp) / 2;
            _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);
            BRN_INFO(";;;;;;;;hnProp for %s: %i", _packet_parameter->get_src_address().unparse().c_str(), hnProp);
            
            if (_ple_interv.mid_now) {
                
                last_hn = _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction();
                hnProp = (last_hn + hnProp) / 2;
                _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);
            }
            
            if (_ple_interv.long_now) {
                
                last_hn = _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction();
                hnProp = (last_hn + hnProp) / 2;
                _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);
            }
        }
    }
}

void PacketLossEstimator::add_ack(EtherAddress dest_addr) {

    BRN_DEBUG("void PacketLossEstimator::add_ack(EtherAddress dest_addr)");
    uint32_t acks = PacketLossEstimator::_acks_by_node.find(dest_addr);
    PacketLossEstimator::_acks_by_node.insert(dest_addr, ++acks);
}

uint32_t PacketLossEstimator::get_acks_by_node(EtherAddress dest_addr) {

    BRN_DEBUG("uint32_t PacketLossEstimator::get_acks_by_node(EtherAddress dest_addr)");
    return PacketLossEstimator::_acks_by_node.find(dest_addr);
}

void PacketLossEstimator::estimateInrange() {

    BRN_DEBUG("void PacketLossEstimator::estimateInrange()");
    if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
        return;
    
    HiddenNodeDetection::NodeInfoTable neighbour_nodes = _hnd->get_nodeinfo_table();
    uint8_t neighbours = 1;
    uint8_t irProp = 6;
    
    for (HiddenNodeDetection::NodeInfoTableIter i = neighbour_nodes.begin(); i != neighbour_nodes.end(); i++) {
        
        if (neighbours >= 255)
            neighbours = 255;
        else
            neighbours++;
    }
    
    uint16_t *backoffsize = _dev->get_cwmax();
    double temp = 1.0;
    
    if (neighbours >= *backoffsize)
        irProp = 100;
    
    else {
        
        for (int i = 1; i < neighbours; i++) {
            
            temp *= (double(*backoffsize) - double(i)) / double(*backoffsize);
        }
        
        irProp = (1 - temp) * 100;
    }
    
    if (irProp < 6)
        irProp = 6;
    
    int last_ir = _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->getFraction();
    irProp = (last_ir + irProp) / 2;
    _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->setFraction(irProp);
    BRN_INFO(";;;;;;;;In-Range1 for %s: %i", _packet_parameter->get_src_address().unparse().c_str(), irProp);
    
    if (_ple_interv.mid_now) {
        
        last_ir = _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->getFraction();
        irProp = (last_ir + irProp) / 2;
        _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->setFraction(irProp);
    }
    
    if (_ple_interv.long_now) {

        last_ir = _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->getFraction();
        irProp = (last_ir + irProp) / 2;
        _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->setFraction(irProp);
    }
}

void PacketLossEstimator::estimateNonWifi(struct airtime_stats *ats) {

    BRN_DEBUG("void PacketLossEstimator::estimateNonWifi(struct airtime_stats *ats)");
    if (_pli != NULL) {
        
        if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
            return;

        if (ats != NULL) {
            
            BRN_INFO("Mac Busy: %d", ats->frac_mac_busy);
            BRN_INFO("Mac RX: %d", ats->frac_mac_rx);
            BRN_INFO("Mac TX: %d", ats->frac_mac_tx);
            BRN_INFO("HW Busy: %d", ats->hw_busy);
            BRN_INFO("HW RX: %d", ats->hw_rx);
            BRN_INFO("HW TX: %d", ats->hw_tx);
            BRN_INFO("Retries: %d", ats->rx_retry_packets);
        }
        
        uint8_t non_wifi = 10;
        if (ats->frac_mac_busy <= ats->hw_busy) {
            non_wifi = ats->hw_busy - ats->frac_mac_busy;
        }
        
        int last_nw = _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->getFraction();
        non_wifi = (last_nw + non_wifi) / 2;
        _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->setFraction(non_wifi);
        BRN_INFO(";;;;;;;;Non-Wifi: %i", non_wifi);

        if (_ple_interv.mid_now) {

            last_nw = _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->getFraction();
            non_wifi = (last_nw + non_wifi) / 2;
            _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->setFraction(non_wifi);
        }

        if (_ple_interv.long_now) {

            last_nw = _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->getFraction();
            non_wifi = (last_nw + non_wifi) / 2;
            _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->setFraction(non_wifi);
        }
    }
}

void PacketLossEstimator::estimateWeakSignal(ChannelStats::SrcInfo *src_info) {

    BRN_DEBUG("void PacketLossEstimator::estimateWeakSignal(ChannelStats::SrcInfo *src_info)");
    if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
        return;
    
    int8_t weaksignal = 0;
    
    if (src_info != NULL) {
        
        if (src_info->_avg_rssi > 0) {
            
            weaksignal = src_info->_avg_rssi - src_info->_std_rssi;
            
            if (weaksignal < 2)
                weaksignal = 100;
            else
                if(weaksignal < 11) // this is only for atheros because RSSI-MAX is differs from vendor to vendor
                    weaksignal = 50;
        }
    }
    
    if (_hnd != NULL) {
        
        HiddenNodeDetection::NodeInfoTable hnd_info_tab = _hnd->get_nodeinfo_table();
        
        for (int cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
        
            EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
            
            if (hnd_info_tab.find(ea) != NULL && !hnd_info_tab.find(ea)->_neighbour)
                _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(100);
            
            else if (ea != _packet_parameter->get_src_address()) {
                
                int last_ws = _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction();
                
                if (last_ws >= 10)
                    _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction((last_ws + last_ws - 10)/2);
                else
                    _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(0);
            }
        }
    }
    
    int last_ws = _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction();
    weaksignal = (last_ws + weaksignal) / 2;
    
    _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(weaksignal);
    BRN_INFO(";;;;;;;;Weak Signal for %s: %i", _packet_parameter->get_src_address().unparse().c_str(), weaksignal);
    
    if (_ple_interv.mid_now) {

        last_ws = _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction();
        weaksignal = (last_ws + weaksignal) / 2;
        _mid_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(weaksignal);
    }

    if (_ple_interv.long_now) {

        last_ws = _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction();
        weaksignal = (last_ws + weaksignal) / 2;
        _long_term_pli.graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(weaksignal);
    }
}

////////////////////////// STATS //////////////////////////////

enum {
    H_STATS
};

String PacketLossEstimator::stats_handler(int mode) {

    StringAccum sa;
    
    sa << "<packetlossreason>\n";
    if (_pli != NULL) {
        
        if (_hnd != NULL) {
            
            HiddenNodeDetection::NodeInfoTable hnd_info_tab = _hnd->get_nodeinfo_table();
            
            const int neighbour_no = hnd_info_tab.size();
            EtherAddress neighbours[neighbour_no];
            
            int cou = 0;
            
            for (HiddenNodeDetection::NodeInfoTableIter i = hnd_info_tab.begin(); i != hnd_info_tab.end(); i++) {
                
                if (cou >= neighbour_no)
                    break;
                
                neighbours[cou++] = i.key();
            }
            
            sa << "\t<hiddennodes>\n";
            
            for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
                
                EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
                
                if (ea == brn_etheraddress_broadcast || hnd_info_tab.find(ea) == NULL || !hnd_info_tab.find(ea)->_neighbour)
                    continue;

                sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
                sa << "\t\t\t<fraction>"
                        << _pli->graph_get(ea)->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction()
                        << "</fraction>\n";

                if (hnd_info_tab.find(ea)->_neighbour) {
                    
                    bool hnds = false;
                    
                    for(HiddenNodeDetection::NodeInfoTableIter itt = hnd_info_tab.find(ea)->_links_to.begin(); itt != hnd_info_tab.find(ea)->_links_to.end(); itt++) {
                        
                        if (!hnd_info_tab.find(itt.key())->_neighbour) {
                            
                            if (!hnds) {
                                
                                sa << "\t\t\t<hidden neighbours>\n";
                                hnds = true;
                            }
                            
                            sa << "\t\t\t\t<address>" << itt.key().unparse().c_str() << "</address>\n";
                        }
                    }
                    
                    if (hnds)
                      sa << "\t\t\t</hidden neighbours>\n";  
                }
                sa << "\t\t</neighbour>\n";
            }
            
            sa << "\t</hiddennodes>\n";
            sa << "\t<inrange>\n";
            
            for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
                
                EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
                
                if (ea == brn_etheraddress_broadcast || hnd_info_tab.find(ea) == NULL || !hnd_info_tab.find(ea)->_neighbour)
                    continue;

                sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
                sa << "\t\t\t<fraction>"
                        << _pli->graph_get(ea)->reason_get(PacketLossReason::IN_RANGE)->getFraction()
                        << "</fraction>\n";
                sa << "\t\t</neighbour>\n";
            }
            
            sa << "\t</inrange>\n";
            sa << "\t<weak_signal>\n";
            
            for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
                
                EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
                
                if (ea == brn_etheraddress_broadcast || hnd_info_tab.find(ea) == NULL)
                    continue;

                sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
                sa << "\t\t\t<fraction>"
                        << _pli->graph_get(ea)->reason_get(PacketLossReason::WEAK_SIGNAL)->getFraction()
                        << "</fraction>\n";
                sa << "\t\t</neighbour>\n";
            }
            sa << "\t</weak_signal>\n";
            sa << "\t<non_wifi>\n";
            
            for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
                
                EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
                
                if (ea == brn_etheraddress_broadcast || hnd_info_tab.find(ea) == NULL || !hnd_info_tab.find(ea)->_neighbour)
                    continue;

                sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
                sa << "\t\t\t<fraction>"
                        << _pli->graph_get(ea)->reason_get(PacketLossReason::NON_WIFI)->getFraction()
                        << "</fraction>\n";
                sa << "\t\t</neighbour>\n";
            }
            sa << "\t</non_wifi>\n";
        }
    }
    
    sa << "</packetlossreason>\n";

    return sa.take_string();
}

static String PacketLossEstimator_read_param(Element *ele, void *thunk) {

    StringAccum sa;
    PacketLossEstimator *ple = (PacketLossEstimator *) ele;
    switch ((uintptr_t) thunk) {

        case H_STATS:
            return ple->stats_handler((uintptr_t) thunk);
            break;
    }

    return String();
}

void PacketLossEstimator::add_handlers() {

    BRNElement::add_handlers();
    add_read_handler("stats", PacketLossEstimator_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PacketLossEstimator)
ELEMENT_MT_SAFE(PacketLossEstimator)
