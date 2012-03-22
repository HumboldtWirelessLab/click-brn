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
    _dev(NULL) {
    
        _packet_parameter = new PacketParameter();
}

PacketLossEstimator::~PacketLossEstimator() {
    
    delete _packet_parameter;
}

HashMap<EtherAddress, uint32_t> PacketLossEstimator::_acks_by_node = HashMap<EtherAddress, uint32_t>();

int PacketLossEstimator::configure(Vector<String> &conf, ErrorHandler* errh) {

    int ret = cp_va_kparse(conf, this, errh,
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

Packet *PacketLossEstimator::simple_action(Packet *packet) {
    
    if (packet != NULL) {
        
        if (_pli != NULL) {
            
            gather_packet_infos_(packet);
            struct airtime_stats *stats;
            HashMap<EtherAddress, ChannelStats::SrcInfo> *src_tab;
            
            if (_cst != NULL) {
                
                stats = _cst->get_latest_stats();
                src_tab = _cst->get_latest_stats_neighbours();
                ChannelStats::SrcInfo srcInfo = src_tab->find(_packet_parameter->get_src_address());
                estimateWeakSignal(&srcInfo);
                estimateNonWifi(stats);
            }
            
            if (_cinfo != NULL && _packet_parameter->get_src_address() != brn_etheraddress_broadcast) {
                
                HashMap<EtherAddress, CollisionInfo::RetryStats*> rs_tab = _cinfo->rs_tab;
                CollisionInfo::RetryStats *rs = rs_tab.find(_packet_parameter->get_src_address());
                
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
                    HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*> neighbours_neighbours = neighbours->get_links_to();
                    BRN_DEBUG("Neighbours of %s", _packet_parameter->get_src_address().unparse().c_str());

                    for (HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*>::iterator i = neighbours_neighbours.begin(); i != neighbours_neighbours.end(); i++) {
                        
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
        return packet;
    }
}

void PacketLossEstimator::gather_packet_infos_(Packet* packet) {

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
                    BRN_DEBUG("unknown subtype: %d", (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK));
            }
            break;

        case WIFI_FC0_TYPE_DATA:
            type.append("DataFrame");
            src_address = EtherAddress(wh->i_addr2);
            packet_type = 30;
            break;

        default:
            BRN_DEBUG("unknown type: %d", (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK));
            src_address = EtherAddress(wh->i_addr2);
    }
    
    if (_debug == 4) {
        
        if (type != NULL)
            BRN_DEBUG("Frame type: %s", type.take_string().c_str());
            
        if (subtype != NULL)
            BRN_DEBUG("Frame subtype: %s", subtype.take_string().c_str());  
    }
    
    _packet_parameter->put_params_(own_address, src_address, dst_address, packet_type);
    
    if (!_pli->mac_address_exists(src_address) && src_address != own_address && src_address != brn_etheraddress_broadcast)
        _pli->graph_insert(src_address);

    if (!_pli->mac_address_exists(dst_address)&& dst_address != own_address && dst_address != brn_etheraddress_broadcast)
        _pli->graph_insert(dst_address);

    if (_debug == 4) {
        
        BRN_DEBUG("Own Address: %s", _packet_parameter->get_own_address().unparse().c_str());
        BRN_DEBUG("Source Address: %s", _packet_parameter->get_src_address().unparse().c_str());
        BRN_DEBUG("Destination Address: %s", _packet_parameter->get_dst_address().unparse().c_str());
    }
}

void PacketLossEstimator::estimateHiddenNode() {
    
    if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
        return;
    
    if (_packet_parameter->get_own_address() != _packet_parameter->get_dst_address()) {
        
        if (_hnd->has_neighbours(_packet_parameter->get_own_address()))
            BRN_DEBUG("Own address has neighbours");
        else
            BRN_DEBUG("Own address has no neighbours");

        uint8_t hnProp = 6;
        bool coopst = false;
        
        if (_hnd->has_neighbours(_packet_parameter->get_src_address())) {
            
            BRN_DEBUG("%s has neighbours", _packet_parameter->get_src_address().unparse().c_str());
            
            HiddenNodeDetection::NodeInfo *neighbour = _hnd->get_nodeinfo_table().find(_packet_parameter->get_src_address());
            HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*> otherNeighbourList = neighbour->get_links_to();
            
            uint8_t neighbours = 0;

            for (HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*>::iterator i = otherNeighbourList.begin(); i != otherNeighbourList.end(); i++) {

                EtherAddress tmpAdd = i.key();
                BRN_DEBUG("Neighbours Neighbour is: %s", tmpAdd.unparse().c_str());

                if (!_hnd->get_nodeinfo_table().find(tmpAdd) || !_hnd->get_nodeinfo_table().find(tmpAdd)->_neighbour) {

                    BRN_DEBUG("Not in my Neighbour List");
                    uint32_t silence_period = Timestamp::now().msec() - _hnd->get_nodeinfo_table().find(_packet_parameter->get_src_address())->get_links_usage().find(tmpAdd).msec();

                    if (neighbours >= 255)
                        neighbours = 255;
                    else
                        neighbours++;

                } else
                    BRN_DEBUG("In my Neighbour List");
            }
            
            if (_cocst != NULL) {
                
                HashMap<EtherAddress, struct neighbour_airtime_stats*> nats_map = _cocst->get_stats(&_packet_parameter->get_src_address());
                
                if (!nats_map.empty()) {
                    
                    BRN_DEBUG("NATS-size: %d", nats_map.size());
                    
                    uint8_t duration = 0;
                    
                    for (HashMap<EtherAddress, struct neighbour_airtime_stats*>::iterator i = nats_map.begin(); i != nats_map.end(); i++) {
                        
                        EtherAddress tmpAdd = i.key();
                        struct neighbour_airtime_stats* nats = i.value();
                        
                        BRN_DEBUG("EA: %s", tmpAdd.unparse().c_str());
                        BRN_DEBUG("NATS-EA: %s", EtherAddress(nats->_etheraddr).unparse().c_str());
                        BRN_DEBUG("PKT-Count: %d", nats->_pkt_count);
                        BRN_DEBUG("Duration: %d", nats->_duration);
                        
                        if (!_hnd->get_nodeinfo_table().find(tmpAdd) || !_hnd->get_nodeinfo_table().find(tmpAdd)->_neighbour)
                            duration += nats->_duration;
                    }
                    
                    if (duration != 0) {
                        
                        hnProp = 1000000/duration;
                        coopst = true;
                    }
                    
                } else
                    BRN_DEBUG("NATS NULL");   
            }
            
            if (!coopst) {
                
                if (_pessimistic_hn_detection && neighbours > 0) { // pessimistic estimator, if we do have one hidden node, he will send as often as possible and as slow as possible. The may interrupt all of our transmissions
                
                    hnProp = 100;
                    
                } else {
                    
                    //TODO: clear stats after 1 sec
                    hnProp = get_acks_by_node(_packet_parameter->get_dst_address()) * 12 /10; // 12 ms per packet / 1000 ms * 100
                }
                
                BRN_DEBUG("%d Acks received for %s", get_acks_by_node(_packet_parameter->get_dst_address()), _packet_parameter->get_dst_address().unparse().c_str());
            }
            
            _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);

            // Problem: hidden nodes who never receive data packets cannot be assigned to any neighbour node. This might be solved with cooperation.
            BRN_DEBUG("hnProp1 for %s: %i", _packet_parameter->get_src_address().unparse().c_str(), hnProp);
        }
    }
}

void PacketLossEstimator::add_ack(EtherAddress dest_addr) {
        
    uint32_t acks = PacketLossEstimator::_acks_by_node.find(dest_addr);
    PacketLossEstimator::_acks_by_node.insert(dest_addr, ++acks);
}
    
uint32_t PacketLossEstimator::get_acks_by_node(EtherAddress dest_addr) {
        
    return PacketLossEstimator::_acks_by_node.find(dest_addr);
}

void PacketLossEstimator::estimateInrange() {
    
    if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
        return;
    
    HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*> neighbour_nodes = _hnd->get_nodeinfo_table();
    uint8_t neighbours = 1;
    uint8_t irProp = 6;
    
    for (HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*>::iterator i = neighbour_nodes.begin(); i != neighbour_nodes.end(); i++) {

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

    _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::IN_RANGE)->setFraction(irProp);
    BRN_DEBUG("In-Range1 for %s: %i", _packet_parameter->get_src_address().unparse().c_str(), irProp);
}

void PacketLossEstimator::estimateNonWifi(struct airtime_stats *ats) {

    if (_pli != NULL) {
        
        if (_packet_parameter->get_src_address() == brn_etheraddress_broadcast || _packet_parameter->get_src_address() == _packet_parameter->get_own_address())
        return;

        if (ats != NULL) {
            
            BRN_DEBUG("Mac Busy: %d", ats->frac_mac_busy);
            BRN_DEBUG("Phy RX: %d", ats->frac_mac_phy_rx);
            BRN_DEBUG("Mac RX: %d", ats->frac_mac_rx);
            BRN_DEBUG("Mac TX: %d", ats->frac_mac_tx);
            BRN_DEBUG("Retries: %d", ats->rx_retry_packets);
        }
        
        uint8_t non_wifi = 10;
        if (ats->frac_mac_rx + ats->frac_mac_tx <= ats->frac_mac_busy) {
            
            non_wifi = ats->frac_mac_rx + ats->frac_mac_tx;
        }
        
        _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::NON_WIFI)->setFraction(non_wifi);
        BRN_DEBUG("Non-Wifi: %i", non_wifi);
    }
}

void PacketLossEstimator::estimateWeakSignal(ChannelStats::SrcInfo *src_info) {
    
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
    
    _pli->graph_get(_packet_parameter->get_src_address())->reason_get(PacketLossReason::WEAK_SIGNAL)->setFraction(weaksignal);
    BRN_DEBUG("Weak Signal for %s: %i", _packet_parameter->get_src_address().unparse().c_str(), weaksignal);
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
            
            sa << "\t<hiddennodes>\n";
            int cou = 0;
            
            for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
                
                EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
                
                if (ea == brn_etheraddress_broadcast)
                    continue;
             
                sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\">\n";
                sa << "\t\t\t<fraction>"
                        << _pli->graph_get(ea)->reason_get(PacketLossReason::HIDDEN_NODE)->getFraction()
                        << "</fraction>\n";
                sa << "\t\t</neighbour>\n";
            }
            
            sa << "\t</hiddennodes>\n";
            sa << "\t<inrange>\n";
            
            for (cou = 0; cou < _pli->node_neighbours_addresses_get().size(); cou++) {
                
                EtherAddress ea = _pli->node_neighbours_addresses_get().at(cou);
                
                if (ea == brn_etheraddress_broadcast)
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
                
                if (ea == brn_etheraddress_broadcast)
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
                
                if (ea == brn_etheraddress_broadcast)
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
