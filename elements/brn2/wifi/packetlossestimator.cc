#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#if CLICK_NS 
  #include <click/router.hh> 
#endif 

#include <iostream>
#include "packetlossestimator.hh"

CLICK_DECLS

PacketLossEstimator::PacketLossEstimator():
	_cst(NULL),
        _cinfo(NULL),
        _hnd(NULL),
        _pli(NULL),
	_dev(NULL),
        _debug(0)
{ /* nothing */ }

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
      if(_debug) {
	
        std::cout << "Sta: " << _dev->getEtherAddress()->unparse() << "\n";
        std::cout << "Dst: " << dst.unparse().c_str() << "\n";
        std::cout << "Busy: " << busy << "  RX: " << rx << "  TX: " << tx << "\n";
        std::cout << "HW_Cycles: " << hw_cycles << "  Busy_Cycles: " << busy_cycles << "  RX_Cycles: " << rx_cycles << "  TX_Cycles: " << tx_cycles << "\n\n\n";
      }
      updatePacketlossStatistics(src, stats);
    }
  }
  return packet;	
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

void PacketLossEstimator::updatePacketlossStatistics(EtherAddress address, int stats[]) {
  
  estimateHiddenNode(address);
  estimateInrange(address, stats);
  estimateNonWifi(stats);
}

 int8_t PacketLossEstimator::addNewPLINode(EtherAddress address) {
  
  if(_pli != NULL) {
    
    if(_pli->graph_get(address) == NULL) {
    
      _pli->graph_insert(address);
      PacketLossInformation_Graph *pliGraph = _pli->graph_get(address);
      pliGraph->graph_build();
      
      if(_debug) {
        std::cout << "PLE::DEBUG: Address inserted: " << address.unparse().c_str() << "\n";
	std::cout << "PLE::DEBUG: pliGraph: " << pliGraph->get_root_node()->getLabel() << "\n";
      }
      return 0;
    } else if(_debug) {
       std::cout << "PLE::DEBUG: Address already exists: " << address.unparse().c_str() << "\n";
      return 1;
    }
  }
  return -1;
}

void PacketLossEstimator::estimateHiddenNode(EtherAddress address) {

  if(_pli != NULL) {
    // Hashmap neighbours (Address
    // neighbour
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
