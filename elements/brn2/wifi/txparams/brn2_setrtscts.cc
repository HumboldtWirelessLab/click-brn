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
#include <clicknet/llc.h>

#include <elements/wifi/wirelessinfo.hh>

#include "brn2_setrtscts.hh"




CLICK_DECLS


	Brn2_SetRTSCTS::Brn2_SetRTSCTS() : _winfo(0),pli(NULL)  {
		_debug = 4;
		_rts = false;
		_mode = WIFI_FC1_DIR_NODS;
		pkt_total = 0;
		
	}


	Brn2_SetRTSCTS::~Brn2_SetRTSCTS() {

	}


  
	int Brn2_SetRTSCTS::initialize(ErrorHandler *) {
		return 0;
	}

	int Brn2_SetRTSCTS::configure(Vector<String> &conf, ErrorHandler* errh) //configure(Vector<String> &, ErrorHandler *) 
	{
		if (cp_va_kparse(conf, this, errh,
      			"PLI", cpkP, cpElement, &pli,
			//"DEBUG", cpkP, cpInteger, &_debug,
		      cpEnd) < 0) return -1;
		return 0;
	}


	/*
	int Brn2_SetRTS::configure(Vector<String> &conf, ErrorHandler *errh)
	{
    	return Args(conf, this, errh).read_mp("RTS", _rts).complete();
	}
	*/

	Packet *Brn2_SetRTSCTS::simple_action(Packet *p)
	{
	  if (p){
		dest_test(p);

		struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
		ceh->magic = WIFI_EXTRA_MAGIC;
		if (rts_get()) {
			ceh->flags |= WIFI_EXTRA_DO_RTS_CTS;
		} else {
	        	ceh->flags &= ~WIFI_EXTRA_DO_RTS_CTS;
    		}
	 }
	 return p;
	}

	enum {brn2_H_RTSCTS};

	static String brn2_SetRTSCTS_read_param(Element *e, void *thunk)
	{
	 	Brn2_SetRTSCTS *td = (Brn2_SetRTSCTS *)e;
		switch ((uintptr_t) thunk) {
			case brn2_H_RTSCTS:
			    return String(td->rts_get()) + "\n";
  			default:
			    return String();
  		}

	}

	static int brn2_SetRTSCTS_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
	{
	  Brn2_SetRTSCTS *f = (Brn2_SetRTSCTS *)e;
	  String s = cp_uncomment(in_s);
	  switch((intptr_t)vparam) {
	  case brn2_H_RTSCTS: {
	    unsigned m;
	    if (!IntArg().parse(s, m))
	      return errh->error("stepup parameter must be unsigned");
	    f->rts_set(m);
	    break;
	  }
	  }
	    return 0;

	}
	void Brn2_SetRTSCTS::add_handlers()
	{
	  add_read_handler("rts", brn2_SetRTSCTS_read_param, brn2_H_RTSCTS);
	  add_write_handler("rts", brn2_SetRTSCTS_write_param, brn2_H_RTSCTS);
	}

	bool Brn2_SetRTSCTS::is_number_in_random_range(unsigned int value)
	{
		int random_number = click_random(0, 100); //generate random number in range [0,100]
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

	bool Brn2_SetRTSCTS::rts_get()
	{
		return _rts;
	}
	void Brn2_SetRTSCTS::rts_set(bool value)
	{
		_rts = value;
	}
	uint32_t Brn2_SetRTSCTS::neighbour_statistic_pkt_total_get() 
	{
		return 1; 
	}
	uint32_t Brn2_SetRTSCTS::neighbour_statistic_rts_on_get() 
	{
		return 2;
	}
	uint32_t Brn2_SetRTSCTS::neighbour_statistc_rts_off_get()
	{
		return (neighbour_statistic_pkt_total_get() - neighbour_statistic_rts_on_get());
	}

	Brn2_SetRTSCTS::PNEIGHBOUR_STATISTICS Brn2_SetRTSCTS::neighbours_statistic_get(EtherAddress dst_address)
	{
		return node_neighbours.get(dst_address);

	}	
	
	void Brn2_SetRTSCTS::print_neighbour_statistics()
	{
		for (HashTable<EtherAddress,PNEIGHBOUR_STATISTICS>::iterator it = node_neighbours.begin(); it; ++it) {
			BRN_DEBUG("pkt_total = %d; rts_on = %d", it.value()->pkt_total, it.value()->rts_on);
		}
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
Packet * Brn2_SetRTSCTS::dest_test(Packet *p)
{

  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid = _winfo ? _winfo->_bssid : _bssid;

  uint16_t ethtype;
  WritablePacket *p_out = 0;

  if (p->length() < sizeof(struct click_ether)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_ether));

    p->kill();
    return 0;

  }

  click_ether *eh = (click_ether *) p->data();
  src = EtherAddress(eh->ether_shost);
  dst = EtherAddress(eh->ether_dhost);
// String s = "hallo:hallo";
//BRN_DEBUG("hallo:In Dest-Test: Destination: %s", s.c_str());


  memcpy(&ethtype, p->data() + 12, 2);

  p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }


  p_out->pull(sizeof(struct click_ether));
  p_out = p_out->push(sizeof(struct click_llc));

  if (!p_out) {
    return 0;
  }

  memcpy(p_out->data(), WIFI_LLC_HEADER, WIFI_LLC_HEADER_LEN);
  memcpy(p_out->data() + 6, &ethtype, 2);

  if (!(p_out = p_out->push(sizeof(struct click_wifi))))
      return 0;
  struct click_wifi *w = (struct click_wifi *) p_out->data();

  memset(p_out->data(), 0, sizeof(click_wifi));
  w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA);
  w->i_fc[1] = 0;
  w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & _mode);


 // BRN_DEBUG("In Dest-Test: Destination: %s; Source: %s", dst.unparse(), src.unparse());

  switch (_mode) {
  case WIFI_FC1_DIR_NODS:
	 BRN_DEBUG("Bin hier in WIFI_FC1_DIR_NODS");

    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
BRN_DEBUG("hallo:In Dest-Test: Destination: %s; Source: %s", dst.unparse().c_str(), src.unparse().c_str());

    break;
  case WIFI_FC1_DIR_TODS:
		 BRN_DEBUG("Bin hier in  WIFI_FC1_DIR_TODS");
    memcpy(w->i_addr1, bssid.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, dst.data(), 6);
    break;
  case WIFI_FC1_DIR_FROMDS:
		 BRN_DEBUG("Bin hier in  WIFI_FC1_DIR_FROMDS");
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, bssid.data(), 6);
    memcpy(w->i_addr3, src.data(), 6);
    break;
  case WIFI_FC1_DIR_DSTODS:
		 BRN_DEBUG("Bin hier in WIFI_FC1_DIR_DSTODS");
    /* XXX this is wrong */
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
    break;
  default:
 click_chatter("%{element}: invalid mode %d\n",
		  this,
		  _mode);
    p_out->kill();
    return 0;
  }
EtherAddress dst_1 = EtherAddress(w->i_addr1);
BRN_DEBUG("In Dest-Test: Destination: %s; ", dst.unparse().c_str());//, src.unparse().c_str());
//if (NULL == pli) BRN_DEBUG("PLI-Pointer is null");
//else if(NULL != pli) {
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
		PacketLossReason *pli_reason =	pli_graph->get_reason_by_name("hidden_node");
		unsigned int frac = pli_reason->getFraction();
		BRN_DEBUG("HIDDEN-NODE-FRACTIOn := %d", frac);
		rts_cts_decision(frac);
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
		//neighbours_statistic_set(dst,ptr_neighbour_stats);
		pkt_total++;
		BRN_DEBUG("Total Number of packets := %d",pkt_total);
		print_neighbour_statistics();
	}
//}
//  BRN_DEBUG("In Dest-Test: Destination: %s; Source: %s", (w->i_addr1).unparse(),(w->i_addr2).unparse());
  return p_out;
}




CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2_SetRTSCTS)
