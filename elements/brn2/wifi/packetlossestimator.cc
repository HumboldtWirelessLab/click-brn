#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/straccum.hh>

#if CLICK_NS 
  #include <click/router.hh> 
#endif 
#include "packetlossestimator.hh"

CLICK_DECLS

PacketLossEstimator::PacketLossEstimator():
	_cst(NULL),
        _cinfo(NULL),
        _hnd(NULL),
        _pli(NULL),
	_dev(NULL)
{
  _debug = 4;
}

PacketLossEstimator::~PacketLossEstimator() { /* nothing */ }

int PacketLossEstimator::configure(Vector<String> &conf, ErrorHandler* errh) {
  int ret = cp_va_kparse(conf, this, errh,
                     "CHANNELSTATS", cpkP, cpElement, &_cst,
                     "COLLISIONINFO", cpkP, cpElement, &_cinfo,
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
  
  if(_pli != NULL && packet != NULL) {
    
    EtherAddress src = getSrcAddress(packet);
    EtherAddress dst = EtherAddress(((struct click_wifi *) packet->data())->i_addr1);
    int packettype = getPacketType(packet);
    if (addNewPLINode(src) > 0) {
      
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
        /*String raw_info = file_string(_proc_file);
        Vector<String> args;

        cp_spacevec(raw_info, args);

        if ( args.size() <= 6 ) return;

        cp_integer(args[1],&busy);
        cp_integer(args[3],&rx);
        cp_integer(args[5],&tx);
        cp_integer(args[7],&hw_cycles);
        cp_integer(args[9],&busy_cycles);
        cp_integer(args[11],&rx_cycles);
        cp_integer(args[13],&tx_cycles);*/

      #endif
      BRN_DEBUG("OwnAddress: %s", _dev->getEtherAddress()->unparse().c_str());
      BRN_DEBUG("SrcAddress: %s", src.unparse().c_str());
      BRN_DEBUG("DstAddress: %s", dst.unparse().c_str());
      BRN_DEBUG("Busy: %d\tRX: %d\tTX: %d", busy, rx, tx);
      BRN_DEBUG("HW_Cycles: %d\tBusy_Cycles: %d\tRX_Cycles: %d\tTX_Cycles: %d\n\n", hw_cycles, busy_cycles, rx_cycles, tx_cycles);
      updatePacketlossStatistics(src, dst, packettype, stats);
    }
  }
  return packet;	
}

uint8_t PacketLossEstimator::getPacketType(Packet *packet) {

  struct click_wifi *wh = (struct click_wifi *) packet->data();
  int returnval = 0;
  switch (wh->i_fc[0] & WIFI_FC0_TYPE_MASK) {

    case WIFI_FC0_TYPE_MGT:
      BRN_DEBUG("ManagementFrame");
      //switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {.... }
      returnval = 10;
      break;
    
    case WIFI_FC0_TYPE_CTL:
      BRN_DEBUG("ControlFrame");
      returnval = 20;
      switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {
      
        case WIFI_FC0_SUBTYPE_PS_POLL:
          BRN_DEBUG("ps-poll");
	  returnval = 21;
          break;
	
        case WIFI_FC0_SUBTYPE_RTS:
          BRN_DEBUG("rts");
	  returnval = 22;
          break;
	
        case WIFI_FC0_SUBTYPE_CTS:	
          BRN_DEBUG("cts");
	  returnval = 23;
          break;
	
        case WIFI_FC0_SUBTYPE_ACK:	
          BRN_DEBUG("ack");
	  returnval = 24;
          break;
	
        case WIFI_FC0_SUBTYPE_CF_END:
          BRN_DEBUG("cf-end");
	  returnval = 25;
          break;
	
        case WIFI_FC0_SUBTYPE_CF_END_ACK:
          BRN_DEBUG("cf-end-ack");
	  returnval = 26;
          break;
	
        default:
          BRN_DEBUG("unknown subtype: %d", (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK));
      }
      break;
    
    case WIFI_FC0_TYPE_DATA:
      BRN_DEBUG("DataFrame");
      returnval = 30;
      break;
    
    default:
      BRN_DEBUG("unknown type: %d", (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK));
    
    return returnval;
  }
}

EtherAddress PacketLossEstimator::getSrcAddress(Packet *packet) {
    
    struct click_wifi *w = (struct click_wifi *) packet->data();
    int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
    
    EtherAddress src;
    switch (type) {
      
      case WIFI_FC0_TYPE_MGT:
      src = EtherAddress(w->i_addr2);
      break;
    case WIFI_FC0_TYPE_CTL:
      src = brn_etheraddress_broadcast;
      break;
    case WIFI_FC0_TYPE_DATA:
      src = EtherAddress(w->i_addr2);
      break;
    default:
      src = EtherAddress(w->i_addr2);
      break;
    }
    
    return src;
}

void PacketLossEstimator::updatePacketlossStatistics(EtherAddress srcAddress, EtherAddress dstAddress, int packettype, int stats[]) {
  
  estimateHiddenNode(srcAddress, dstAddress, packettype);
  estimateInrange(srcAddress, stats);
  estimateNonWifi(stats);
}

 int8_t PacketLossEstimator::addNewPLINode(EtherAddress address) {
  
  if(_pli != NULL) {
    
    if(_pli->graph_get(address) == NULL) {
       
      _pli->graph_insert(address);
      PacketLossInformation_Graph *pliGraph = _pli->graph_get(address);
      
      BRN_DEBUG("Address inserted: %s", address.unparse().c_str());
      //BRN_DEBUG("PLE::DEBUG: pliGraph: %s", pliGraph->get_root_node()->getLabel());
      
      return 0;
    } else {
      
      BRN_DEBUG("Address already exists: %s", address.unparse().c_str());
      return 1;
    }
  }
  return -1;
}

void PacketLossEstimator::estimateHiddenNode(EtherAddress srcAddress, EtherAddress dstAddress, int packettype) {
  
  if(_pli != NULL) {
    
    if(packettype < 25 && packettype > 20) {
      
      BRN_DEBUG("ControllFrame catched");
      // Hashmap neighbours (Address
      // neighbour
      
      // pro nachbarn: wenn ich Daten-pakete mit src(Nachbar)->dst(x) höre -> Pakete mit src(x)->dst(Nachbar)? wenn ja, kein HN
      // wenn nein, höre ich Acks addr1(x)? wenn nein, Knoten x nich da. Wenn ja, HN. HN(b) > 0
      // Datenpakete + ACKS pro Zeit für Prozent Kanalbelegung -> HN-Wahrscheinlichkeit , Für mehere HN Wahrscheinlichkeiten addieren.

	
      // prüfen, ob ack für möglichen HN-Kandidaten eingetroffen, wenn ja, dann HN anpassen
    } else {
      BRN_DEBUG("Other Frame catched");
      if (*_dev->getEtherAddress() != dstAddress) {
	
	addAddress2NodeList(srcAddress, dstAddress);
      } 
    }
  } else {
    // Beschweren, dass PLI NULL
  }
}

void PacketLossEstimator::addAddress2NodeList(EtherAddress srcAddress, EtherAddress dstAddress){
  BRN_DEBUG("Add Address 2 NodeList");
  if (srcAddress != brn_etheraddress_broadcast) {
  
    if (nodeList.contains(srcAddress)) {
      
      nodeList.updateNeighbours(srcAddress, dstAddress);
      
    } else {
      nodeList.add(srcAddress, dstAddress);
    }
    uint8_t hnProp = nodeList.calcHiddenNodesProbability(*_dev->getEtherAddress(), srcAddress);
    _pli->graph_get(srcAddress)->get_reason_by_id(PacketLossReason::HIDDEN_NODE)->setFraction(hnProp);
    BRN_DEBUG("hnProp: %i", hnProp);
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
    if (as->hw_available) {
      sa << "true\" />\n";
    } else {
      sa << "false\" />\n";
    }
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
