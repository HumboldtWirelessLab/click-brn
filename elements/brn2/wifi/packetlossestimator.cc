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

PacketLossEstimator::PacketLossEstimator():
    _cst(NULL),
    _cinfo(NULL),
    _hnd(NULL),
    _pli(NULL),
    _dev(NULL)
{
    _debug = 4;
    packet_parameter = new PacketParameter();
}

PacketLossEstimator::~PacketLossEstimator() { 
    delete packet_parameter;
}

int PacketLossEstimator::configure(Vector<String> &conf, ErrorHandler* errh) {
    
    int ret = cp_va_kparse(conf, this, errh,
        "CHANNELSTATS", cpkP, cpElement, &_cst,
        "COLLISIONINFO", cpkP, cpElement, &_cinfo,
        "HIDDENNODE", cpkP, cpElement, &_hnd,
        "PLI", cpkP, cpElement, &_pli,
        "DEVICE", cpkP, cpElement, &_dev,
        "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd);
    
    return ret;
}

int PacketLossEstimator::initialize(ErrorHandler *errh) {
    
    return 0;
}

Packet *PacketLossEstimator::simple_action(Packet *packet) {
    
    if(packet != NULL) {
        
        gather_packet_infos_(packet);
    
        if(_pli != NULL) {
            
            if(_cst != NULL) {
                
                BRN_DEBUG("CST NOT NULL");
            }
            
            if(_cinfo != NULL && packet_parameter->get_src_address() != brn_etheraddress_broadcast) {
                
                HashMap<EtherAddress, CollisionInfo::RetryStats*> rs_tab =_cinfo->rs_tab;
                CollisionInfo::RetryStats *rs = rs_tab.find(packet_parameter->get_src_address());
                
                if (rs != NULL) {
                    BRN_DEBUG("CINFO fraction for %s: %d", packet_parameter->get_src_address().unparse().c_str(), rs->get_frac(0));
                }
                else {
                    BRN_DEBUG("CINFO fraction for %s is NULL!!!", packet_parameter->get_src_address().unparse().c_str());
                }
            }
            
            if(_hnd != NULL && packet_parameter->get_src_address() != brn_etheraddress_broadcast) {
                
                if(_hnd->has_neighbours(packet_parameter->get_src_address())) {
                    
                    HiddenNodeDetection::NodeInfo* neighbours = _hnd->get_nodeinfo_table().find(packet_parameter->get_src_address());                
                    HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*> neighbours_neighbours = neighbours->get_links_to();
                    BRN_DEBUG("Neighbours of %s", packet_parameter->get_src_address().unparse().c_str());
                    
                    for(HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*>::iterator i = neighbours_neighbours.begin(); i != neighbours_neighbours.end(); i++) {
                        BRN_DEBUG("  Neighbour: %s", i.key().unparse().c_str());
                    }
                } else {
                    
                  BRN_DEBUG("%s has no neighbours", packet_parameter->get_src_address().unparse().c_str());
                }
            }
    /*        
            if (addNewPLINode() > 0) {
          */  
                int busy, rx, tx;
                uint32_t hw_cycles, busy_cycles, rx_cycles, tx_cycles;
                int stats[16];
                #if CLICK_NS
                    
                    simclick_sim_command(router()->simnode(), SIMCLICK_CCA_OPERATION, &stats);

                    busy = stats[0];
                    rx = stats[1];
                    tx = stats[2];

                    hw_cycles = stats[3];
                    busy_cycles = stats[4];
                    rx_cycles = stats[5];
                    tx_cycles = stats[6];
                #else
        /*
                    String raw_info = file_string(_proc_file);
                    Vector<String> args;

                    cp_spacevec(raw_info, args);

                    if ( args.size() <= 6 ) return;

                    cp_integer(args[1],&busy);
                    cp_integer(args[3],&rx);
                    cp_integer(args[5],&tx);
                    cp_integer(args[7],&hw_cycles);
                    cp_integer(args[9],&busy_cycles);
                    cp_integer(args[11],&rx_cycles);
                    cp_integer(args[13],&tx_cycles);
     */ 
                #endif
                
                BRN_DEBUG("Busy: %d\tRX: %d\tTX: %d", busy, rx, tx);
                BRN_DEBUG("HW_Cycles: %d\tBusy_Cycles: %d\tRX_Cycles: %d\tTX_Cycles: %d\n\n", hw_cycles, busy_cycles, rx_cycles, tx_cycles);
           
                updatePacketlossStatistics(stats);

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
    
    switch (wh->i_fc[0] & WIFI_FC0_TYPE_MASK) {
    
        case WIFI_FC0_TYPE_MGT:
            BRN_DEBUG("ManagementFrame");
            src_address = EtherAddress(wh->i_addr2);
            //switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {.... }
            packet_type = 10;
            break;
        
        case WIFI_FC0_TYPE_CTL:
            BRN_DEBUG("ControlFrame");
            src_address = brn_etheraddress_broadcast;
            packet_type = 20;
            switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {
            
                case WIFI_FC0_SUBTYPE_PS_POLL:
                    BRN_DEBUG("ps-poll");
                    packet_type = 21;
                    break;
                
                case WIFI_FC0_SUBTYPE_RTS:
                    BRN_DEBUG("rts");
                    packet_type = 22;
                    break;
                
                case WIFI_FC0_SUBTYPE_CTS:      
                    BRN_DEBUG("cts");
                    packet_type = 23;
                    break;
                
                case WIFI_FC0_SUBTYPE_ACK:      
                    BRN_DEBUG("ack");
                    packet_type = 24;
                    break;
                
                case WIFI_FC0_SUBTYPE_CF_END:
                    BRN_DEBUG("cf-end");
                    packet_type = 25;
                    break;
                
                case WIFI_FC0_SUBTYPE_CF_END_ACK:
                    BRN_DEBUG("cf-end-ack");
                    packet_type = 26;
                    break;
                
                default:
                    BRN_DEBUG("unknown subtype: %d", (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK));
            }
            break;
        
        case WIFI_FC0_TYPE_DATA:
            BRN_DEBUG("DataFrame");
            src_address = EtherAddress(wh->i_addr2);
            packet_type = 30;
            break;
        
        default:
            BRN_DEBUG("unknown type: %d", (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK));
            src_address = EtherAddress(wh->i_addr2);
    }
    packet_parameter->put_params_(own_address, src_address, dst_address, packet_type);
    
    BRN_DEBUG("Own Address: %s", packet_parameter->get_own_address().unparse().c_str());
    BRN_DEBUG("Source Address: %s", packet_parameter->get_src_address().unparse().c_str());
    BRN_DEBUG("Destination Address: %s", packet_parameter->get_dst_address().unparse().c_str());
}

void PacketLossEstimator::updatePacketlossStatistics(int stats[]) {
    
    estimateHiddenNode();
    //estimateInrange(packet_parameter->get_src_address(), stats);
    //estimateNonWifi(stats);
}

int8_t PacketLossEstimator::addNewPLINode() {
    /*
    if(_pli->graph_get(packet_parameter->get_src_address()) == NULL) {
        
        _pli->graph_insert(packet_parameter->get_src_address());
        PacketLossInformation_Graph *pliGraph = _pli->graph_get(packet_parameter->get_src_address());
            
        BRN_DEBUG("Address inserted: %s", packet_parameter->get_src_address().unparse().c_str());
        //BRN_DEBUG("PLE::DEBUG: pliGraph: %s", pliGraph->get_root_node()->getLabel());
            
        return 0;
    } else {
    
        BRN_DEBUG("Address already exists: %s", packet_parameter->get_src_address().unparse().c_str());
        return 1;
    }
    */
    return -1;
}

int inline fac(int n){
    
    int res = 1;
    
    while (n > 0) {    
            res=res*n;    
            n=n-1;
    }
    
    return res;
}

void PacketLossEstimator::estimateHiddenNode( ) {
    
    if (packet_parameter->get_own_address() != packet_parameter->get_dst_address()) {
        
        if(_hnd->has_neighbours(packet_parameter->get_own_address()))
            BRN_DEBUG("Own address has neighbours");
        else
            BRN_DEBUG("Own address has no neighbours");
        
        uint8_t hnProp = 6;
        if(_hnd->has_neighbours(packet_parameter->get_src_address())) {
            
            BRN_DEBUG("%s has neighbours", packet_parameter->get_src_address().unparse().c_str());
            
            HiddenNodeDetection::NodeInfo *neighbour = _hnd->get_nodeinfo_table().find(packet_parameter->get_src_address());
            HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*> otherNeighbourList = neighbour->get_links_to();
            
            uint8_t neighbours = 0;
            
            for(HashMap<EtherAddress, HiddenNodeDetection::NodeInfo*>::iterator i = otherNeighbourList.begin(); i != otherNeighbourList.end(); i++) {
                
                EtherAddress tmpAdd = i.key();
                BRN_DEBUG("Neighbours Neighbour is: %s", tmpAdd.unparse().c_str());
                
                if(!_hnd->get_nodeinfo_table().find(tmpAdd)) {
                    
                    BRN_DEBUG("Not in my Neighbour List");
                    uint32_t silence_period = Timestamp::now().msec() - _hnd->get_nodeinfo_table().find(packet_parameter->get_src_address())->get_links_usage().find(tmpAdd).msec();
                    
                    if(neighbours >= 255) {
                        neighbours = 255;
                    } else {
                        neighbours++;
                    }
                        //hnProp += i.value()->;
                        
                        //1 - ((Zeit)!/((Zeit-Nachbarn)! * Zeit ^Nachbarn)) Zeit = 10 ms (besser backoffzeiten und sendehäufigkeit mit einbeziehen)
                } else {
                    BRN_DEBUG("In my Neighbour List");
                }
            }
            int period = 10;
            
            hnProp = 1 - (fac(period)/(fac(period - neighbours) * pow(period, neighbours)));
            if (hnProp < 6) hnProp = 6;
        }
        
        //_pli->graph_get(packet_parameter->get_src_address())->get_reason_by_id(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);
        BRN_DEBUG("hnProp: %i", hnProp);

//         if(packet_parameter->get_packet_type() < 25 && packet_parameter->get_packet_type() > 20) {
//             
//             BRN_DEBUG("Control Frame catched");
//             
//             if (packet_parameter->get_packet_type() == 24) {
//                 
//                 handleAckFrame(packet_parameter->get_dst_address());
//             }
//             // pro nachbarn: wenn ich Daten-pakete mit src(Nachbar)->dst(x) höre -> Pakete mit src(x)->dst(Nachbar)? wenn ja, kein HN
//             // wenn nein, höre ich Acks addr1(x)? wenn nein, Knoten x nich da. Wenn ja, HN. HN(b) > 0
//             // Datenpakete + ACKS pro Zeit für Prozent Kanalbelegung -> HN-Wahrscheinlichkeit , Für mehere HN Wahrscheinlichkeiten addieren.
//                     
//             // prüfen, ob ack für möglichen HN-Kandidaten eingetroffen, wenn ja, dann HN anpassen
//         } else {
//             
//             addAddress2NodeList(packet_parameter->get_src_address(), packet_parameter->get_dst_address());    
//         }
        BRN_DEBUG("leave estimateHiddenNode");
    }
}

void PacketLossEstimator::addAddress2NodeList(EtherAddress srcAddress, EtherAddress dstAddress){
    
    BRN_DEBUG("Add Address 2 NodeList");
    if (srcAddress != brn_etheraddress_broadcast) {
        
        if(nodeList.contains(srcAddress))
            nodeList.updateNeighbours(srcAddress, dstAddress);
      
        else
            nodeList.add(srcAddress, dstAddress);
    
        uint8_t hnProp = nodeList.calcHiddenNodesProbability(*_dev->getEtherAddress(), srcAddress);
        _pli->graph_get(srcAddress)->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);
        BRN_DEBUG("hnProp: %i", hnProp);
    }
}

void PacketLossEstimator::handleAckFrame(EtherAddress dstAddress) {
    
    if(!nodeList.hasNeighbour(*_dev->getEtherAddress(), dstAddress)) {
        
        Vector<EtherAddress> addresses = nodeList.addAck(dstAddress);
        for(Vector<EtherAddress>::iterator i = addresses.begin(); i != addresses.end(); i++) {
            
            uint8_t hnProp = nodeList.calcHiddenNodesProbability(*_dev->getEtherAddress(), *i);
            _pli->graph_get(*i)->reason_get(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);    
        }
    }
}

void PacketLossEstimator::estimateInrange(EtherAddress address, int stats[]) {
    
    if(_pli != NULL) {
        
    }
}

void PacketLossEstimator::estimateNonWifi(int stats[]) {
    
    if(_pli != NULL) {
        
    }
}

enum {H_STATS};

String PacketLossEstimator::stats_handler(int mode) {
    
    StringAccum sa;
    struct airtime_stats *as;

    sa << "<packetlossreason>\n";
    if(_cst != NULL) {
        
        as = _cst->get_latest_stats();
        sa << "\t<rxPackets>" << as->rxpackets << "</rxPackets>\n";
        sa << "\t<txPackets>" << as->txpackets << "</txPackets>\n";
        sa << "\t<hwInfo awailable=\"";
        if (as->hw_available)
            sa << "true\" />\n";
        else
            sa << "false\" />\n";
        
    }
    sa << "</packetlossreason>\n";
    return  sa.take_string();
}

static String PacketLossEstimator_read_param(Element *ele, void *thunk) {
  
    StringAccum sa;
    PacketLossEstimator *ple = (PacketLossEstimator *)ele;
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
