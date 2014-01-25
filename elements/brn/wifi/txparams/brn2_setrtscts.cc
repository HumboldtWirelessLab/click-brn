/*
 *  
 */

#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "brn2_setrtscts.hh"



CLICK_DECLS
	
Brn2_SetRTSCTS::Brn2_SetRTSCTS():
  _rts_cts_strategy(RTS_CTS_STRATEGY_ALWAYS_OFF),
  pli(NULL)
{
	_rts = false;
	pkt_total = 0;
	
}

Brn2_SetRTSCTS::~Brn2_SetRTSCTS() {
}

void Brn2_SetRTSCTS::address_broadcast_insert()
{
	EtherAddress broadcast_address = EtherAddress();
	broadcast_address = broadcast_address.make_broadcast();
	neighbours_statistic_insert(broadcast_address); 
	BRN_DEBUG("Ist Pointer Address? %d",broadcast_address.is_broadcast());
}

int Brn2_SetRTSCTS::initialize(ErrorHandler *) {
	address_broadcast_insert();
	return 0;
}

int Brn2_SetRTSCTS::configure(Vector<String> &conf, ErrorHandler* errh) 
{
	if (cp_va_kparse(conf, this, errh,
    "STRATEGY", cpkP, cpInteger, &_rts_cts_strategy,
		"PLI", cpkP, cpElement, &pli,
		"DEBUG", cpkP, cpInteger, &_debug,
	      cpEnd) < 0) return -1;
	return 0;
}

Packet *Brn2_SetRTSCTS::simple_action(Packet *p)
{
  if (p) {
    bool set_rtscts = false;
    EtherAddress dst = EtherAddress(BRNPacketAnno::dst_ether_anno(p));

    switch ( _rts_cts_strategy ) {
      case RTS_CTS_STRATEGY_UNICAST_ON:
        if (!dst.is_broadcast()) set_rtscts = true;
        break;
      case RTS_CTS_STRATEGY_ALWAYS_ON:
        set_rtscts = true;
        break;
      case RTS_CTS_STRATEGY_SIZE_LIMIT:
        break;
      case RTS_CTS_STRATEGY_RANDOM:
        break;
      case RTS_CTS_STRATEGY_PLI:
        dest_test(p);
      case RTS_CTS_STRATEGY_HIDDENNODE:
      case RTS_CTS_STRATEGY_ALWAYS_OFF:
      default:
        break;
    }

    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
    ceh->magic = WIFI_EXTRA_MAGIC;
    if (set_rtscts) ceh->flags |= WIFI_EXTRA_DO_RTS_CTS;
    else 	ceh->flags &= ~WIFI_EXTRA_DO_RTS_CTS;
  }

  return p;
}

bool Brn2_SetRTSCTS::is_number_in_random_range(unsigned int value)
{
	unsigned int random_number = click_random(0, 100); //generate random number in range [0,100]
	BRN_DEBUG("Random_number = %d", random_number);
	if (random_number <= value) return true;
	else {
		return false;
	}
}
/* true = on, else off*/
void Brn2_SetRTSCTS::rts_cts_decision(unsigned int value) 
{
	//int value = 5;//hier von hidden node abhängig
	if (is_number_in_random_range(value)) {
		rts_set(true);
	}
	else {
		rts_set(false);
	}	
}

Packet* Brn2_SetRTSCTS::dest_test(Packet *p)
{
    // Get destination mac-address
	struct click_wifi *wh = (struct click_wifi *) p->data();
  	EtherAddress src;
  	EtherAddress dst;
 	EtherAddress bssid;
 	switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  		case WIFI_FC1_DIR_NODS:
    			dst = EtherAddress(wh->i_addr1);
    			src = EtherAddress(wh->i_addr2);
    			bssid = EtherAddress(wh->i_addr3);
    		break;
  		case WIFI_FC1_DIR_TODS:
    			bssid = EtherAddress(wh->i_addr1);
    			src = EtherAddress(wh->i_addr2);
    			dst = EtherAddress(wh->i_addr3);
    		break;
  		case WIFI_FC1_DIR_FROMDS:
    			dst = EtherAddress(wh->i_addr1);
    			bssid = EtherAddress(wh->i_addr2);
    			src = EtherAddress(wh->i_addr3);
    		break;
  		case WIFI_FC1_DIR_DSTODS:
    			dst = EtherAddress(wh->i_addr1);
    			src = EtherAddress(wh->i_addr2);
    			bssid = EtherAddress(wh->i_addr3);
    		break;
  		default:
  		BRN_DEBUG("Packet-Mode is unknown");
  	}
	BRN_DEBUG("In Dest-Test: Destination: %s; ", dst.unparse().c_str());//, src.unparse().c_str());
	if(NULL != pli) {
		BRN_DEBUG("Before pli_graph");
		PacketLossInformation_Graph *pli_graph = pli->graph_get(dst);
		BRN_DEBUG("AFTER pli_graph");
		if(NULL == pli_graph) {
			BRN_DEBUG("There is not a Graph available for the DST-Adress: %s", dst.unparse().c_str());
			pli->graph_insert(dst);
			BRN_DEBUG("Destination-Adress was inserted ");
		}
		else {
	 		BRN_DEBUG("There is a Graph available for the DST-Adress: %s", dst.unparse().c_str());
			PacketLossReason *pli_reason =	pli_graph->reason_get("hidden_node");
			unsigned int frac = pli_reason->getFraction();
			BRN_DEBUG("HIDDEN-NODE-FRACTION := %d", frac);
            // decide if RTS/CTS is on or off
			rts_cts_decision(frac);
            // Do something for the Statistic-Element 
			PNEIGHBOUR_STATISTICS ptr_neighbour_stats = neighbours_statistic_get(dst);
			if(NULL == ptr_neighbour_stats) {
				neighbours_statistic_insert(dst);
				ptr_neighbour_stats = neighbours_statistic_get(dst);
			}
			ptr_neighbour_stats->pkt_total++;
			if (rts_get()) {
				BRN_DEBUG("RTS-CTS on");
				ptr_neighbour_stats->rts_on = ptr_neighbour_stats->rts_on + 1;
			}
			else {
				BRN_DEBUG("RTS-CTS off");
			}
			pkt_total++;
			BRN_DEBUG("Total Number of packets := %d",pkt_total);
		}
	}
  	return p;
}


bool Brn2_SetRTSCTS::rts_get()
{
	return _rts;
}
void Brn2_SetRTSCTS::rts_set(bool value)
{
	_rts = value;
}
uint32_t Brn2_SetRTSCTS::neighbour_statistic_pkt_total_get(EtherAddress dst_address) 
{
	PNEIGHBOUR_STATISTICS ptr_neighbour_stats = neighbours_statistic_get(dst_address);
	return ptr_neighbour_stats->pkt_total;
}
uint32_t Brn2_SetRTSCTS::neighbour_statistic_rts_on_get(EtherAddress dst_address)
{
	PNEIGHBOUR_STATISTICS ptr_neighbour_stats = neighbours_statistic_get(dst_address);
	return ptr_neighbour_stats->rts_on;
}
uint32_t Brn2_SetRTSCTS::neighbour_statistc_rts_off_get(EtherAddress dst_address)
{
	return (neighbour_statistic_pkt_total_get(dst_address) - neighbour_statistic_rts_on_get(dst_address));
}

Brn2_SetRTSCTS::PNEIGHBOUR_STATISTICS Brn2_SetRTSCTS::neighbours_statistic_get(EtherAddress dst_address)
{
	return node_neighbours.get(dst_address);

}	

String  Brn2_SetRTSCTS::print()
{
	StringAccum sa;
	sa << "<brnelement name=\"Brn2_SetRTSCTS\" myaddress=\""<< BRN_NODE_NAME << "\">\n";
	for (HashTable<EtherAddress,PNEIGHBOUR_STATISTICS>::iterator it = node_neighbours.begin(); it; ++it) {
		sa << "\t<neighbour address=\"" << (it.key()).unparse().c_str() <<"\">\n";
		sa << "\t\t<information_sending packets_total=\"" << it.value()->pkt_total << "\" rts_on=\"" << it.value()->rts_on;
		sa << "\" rts_off=\"" << (it.value()->pkt_total - it.value()->rts_on) <<"\"/>\n";
		sa.append("\t</neighbour>\n");
	}
	sa.append("</brnelement>\n");
	return sa.take_string();
}

void Brn2_SetRTSCTS::neighbours_statistic_set(EtherAddress dst_address,PNEIGHBOUR_STATISTICS ptr_neighbour_stats)
{
	node_neighbours[dst_address] = ptr_neighbour_stats;

}

void Brn2_SetRTSCTS::neighbours_statistic_insert(EtherAddress dst_address) 
{
	PNEIGHBOUR_STATISTICS ptr_neighbour_stats = new neighbour_statistics();
	node_neighbours[dst_address] = ptr_neighbour_stats;

}

void Brn2_SetRTSCTS::strategy_set(uint16_t value)
{
	_rts_cts_strategy = value;
}

uint16_t Brn2_SetRTSCTS::strategy_get()
{
	return _rts_cts_strategy;
}

void Brn2_SetRTSCTS::reset()
{
	node_neighbours.clear();
	address_broadcast_insert();
}

enum {H_RTSCTS_PRINT, H_RTSCTS_RESET, H_RTSCTS_ALWAYS_ON, H_RTSCTS_ALWAYS_OFF, H_RTSCTS_RANDOM};

static String SetRTSCTS_read_param(Element *e, void *thunk)
{
	Brn2_SetRTSCTS *f = (Brn2_SetRTSCTS *)e;
	switch ((uintptr_t) thunk) {
		case H_RTSCTS_PRINT:
			return  f->print(); 
  			default:
				return String();
  	}
}


static int SetRTSCTS_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler * /*errh*/)
{
	Brn2_SetRTSCTS *f = (Brn2_SetRTSCTS *)e;
	String s = cp_uncomment(in_s);
	switch((intptr_t)vparam) {
	  	case H_RTSCTS_RESET: 
			f->reset();
			break;		
		case H_RTSCTS_ALWAYS_ON: 
	    		f->strategy_set(RTS_CTS_STRATEGY_ALWAYS_ON);
	    		break;
		case H_RTSCTS_ALWAYS_OFF: 
	    		f->strategy_set(RTS_CTS_STRATEGY_ALWAYS_OFF);
	    		break;
		case H_RTSCTS_RANDOM: 
	    		f->strategy_set(RTS_CTS_STRATEGY_RANDOM);
	    		break;
	}
	return 0;
}


void Brn2_SetRTSCTS::add_handlers()
{
	BRNElement::add_handlers();//for Debug-Handlers
	add_read_handler("print",SetRTSCTS_read_param,H_RTSCTS_PRINT);

	add_write_handler("reset",SetRTSCTS_write_param,H_RTSCTS_RESET, Handler::h_button);//see include/click/handler.hh 
	add_write_handler("strategy_always_off", SetRTSCTS_write_param,H_RTSCTS_ALWAYS_OFF);
	add_write_handler("strategy_always_on", SetRTSCTS_write_param, H_RTSCTS_ALWAYS_ON);
	add_write_handler("strategy_always_random", SetRTSCTS_write_param, H_RTSCTS_RANDOM);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2_SetRTSCTS)
