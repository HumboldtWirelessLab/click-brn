/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * tos2queuemapper.{cc,hh}
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "tos2queuemapper.hh"

CLICK_DECLS

#include "tos2queuemapper_data.hh"

Tos2QueueMapper::Tos2QueueMapper():
    _cst(NULL),    //Channelstats-Element
    _colinf(NULL), //Collission-Information-Element
    pli(NULL),      //PacketLossInformation-Element
    _bqs_strategy(BACKOFF_STRATEGY_OFF)
{
  BRNElement::init();
  _likelihood_collison = -1;
  _rate = -1;
  _msdu_size = -1;
  _index_packet_loss_max = 5;
  _index_number_of_neighbours_max = 25;
  _index_msdu_size_max = 4;
  _index_rates_max = 4;
}

Tos2QueueMapper::~Tos2QueueMapper()
{
}

int
Tos2QueueMapper::configure(Vector<String> &conf, ErrorHandler* errh)
{
	String s_cwmin = "";
	String s_cwmax = "";
	String s_aifs = "";
	int v;

	if (cp_va_kparse(conf, this, errh,
	      	"CWMIN", cpkP, cpString, &s_cwmin,
	      	"CWMAX", cpkP, cpString, &s_cwmax,
	      	"AIFS", cpkP, cpString, &s_aifs,
	      	"CHANNELSTATS", cpkP, cpElement, &_cst,
	      	"COLLISIONINFO", cpkP, cpElement, &_colinf,
	      	"PLI", cpkP, cpElement, &pli,
                "LIKELIHOODCOLLISON", cpkP, cpInteger, &_likelihood_collison,
                "RATE", cpkP, cpInteger, &_rate,
                "MSDUSIZE", cpkP, cpInteger, &_msdu_size,
		"STRATEGY", cpkP, cpInteger, &_bqs_strategy,
	      	"DEBUG", cpkP, cpInteger, &_debug,
	      	cpEnd) < 0) return -1;


	Vector<String> args;
	cp_spacevec(s_cwmin, args);
  	no_queues = args.size();
  	if ( no_queues > 0 ) {
		_cwmin = new uint16_t[no_queues];
	    	_cwmax = new uint16_t[no_queues];
	    	_aifs = new uint16_t[no_queues];

#pragma message "TODO: Better check for params. Better Error handling."
	    	for( int i = 0; i < no_queues; i++ ) {
	      		cp_integer(args[i], &v);
	      		_cwmin[i] = v;
        }
        args.clear();
		cp_spacevec(s_cwmax, args);
	    	if ( args.size() < no_queues ) no_queues = args.size();
	    	for( int i = 0; i < no_queues; i++ ) {
	      		cp_integer(args[i], &v);
	      		_cwmax[i] = v;
	    	}
        args.clear();
	    	cp_spacevec(s_aifs, args);
	    	if ( args.size() < no_queues ) no_queues = args.size();
	    	for( int i = 0; i < no_queues; i++ ) {
	      		cp_integer(args[i], &v);
	     		_aifs[i] = v;
	    	}
        args.clear();
	}
  	_queue_usage = new uint32_t[no_queues];
	reset_queue_usage();
	
	
	
//	for ( int i = 0; i < 25; i++) {
//	  BRN_ERROR("N: %d backoff: %d",i,_backoff_matrix_tmt_backoff_3D[0 /*rate 1*/][1/*msdu 1500*/][i]);//
//	}
  	return 0;
}

uint8_t Tos2QueueMapper::no_queues_get()
{
	return	no_queues;
}

uint32_t Tos2QueueMapper::queue_usage_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_queue_usage[position];
}

uint32_t Tos2QueueMapper::cwmin_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues -1;
	return	_cwmin[position];
}

uint32_t Tos2QueueMapper::cwmax_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_cwmax[position];
}

int Tos2QueueMapper::backoff_strategy_neighbours_pli_aware(Packet *p,uint8_t tos)
{
    int32_t first_time = 0;
    //uint32_t fraction  = 0; veraltet
    int32_t number_of_neighbours = 0;
    int32_t  index_search_rate = -1;
    int32_t  index_search_msdu_size = -1;
    int32_t  index_search_likelihood_collision = -1;
    int32_t  index_no_neighbours = -1;
    uint32_t  backoff_window_size = 0;
    int backoff_window_size_2 = -1;
    int opt_queue = -1;
    // Get Destination Address from the current packet
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
    // Get Fraction for inrange collisions for the current Destination Address  Code veraltet
    /*if(NULL != pli) {
        BRN_DEBUG("Before pli_graph");
        PacketLossInformation_Graph *pli_graph = pli->graph_get(dst);
        BRN_DEBUG("AFTER pli_graph");
        if(NULL != pli_graph) {
            BRN_DEBUG("There is not a Graph available for the DST-Address: %s", dst.unparse().c_str());
            PacketLossReason* pli_reason = pli_graph->reason_get("in_range");
            fraction = pli_reason->getFraction();                    
            BRN_DEBUG("In-Range-Fraction := %d", fraction);
        }
        else {
            fraction = _backoff_packet_loss[packet_loss_index_max];
        }
    }
    else {
            fraction = _backoff_packet_loss[packet_loss_index_max];
    }*/
    //Get Number of Neighbours from the Channelstats-Element (_cst)
    if ( NULL != _cst ) {
	    struct airtime_stats *as;
		as = _cst->get_latest_stats(); // _cst->get_stats(&as,0);//get airtime statisics
        number_of_neighbours = as->no_sources;
		BRN_DEBUG("Number of Neighbours %d", number_of_neighbours);  
	}
    if (_rate > 0){
        for (int i = 0; i <= _index_rates_max;i++) {
            if (vector_rates_data[i] == _rate) {
                index_search_rate = i;
            }
        }
    }
    if (_msdu_size > 0){
        for (int i = 0; i <= _index_msdu_size_max;i++) {
            if (vector_msdu_sizes[i] == _msdu_size) {
                index_search_msdu_size = i;
            }
        }
    }
    if (_likelihood_collison > 0){
        for (int i = 0; i <= _index_packet_loss_max;i++) {
            if (_backoff_packet_loss[i] == _likelihood_collison) {
                index_search_likelihood_collision = i;
            }
        }
    }
    if (number_of_neighbours > 0){
        for (int i = 0; i <= _index_number_of_neighbours_max;i++) {
            if (vector_no_neighbours[i] == number_of_neighbours) {
                index_no_neighbours = i;
            }
        }
    }
    // Tests what is known
    if ( index_search_rate >= 0 && index_search_msdu_size >= 0 && index_search_likelihood_collision >= 0 && index_no_neighbours >= 0 && first_time == 0) {
       backoff_window_size_2 = _backoff_matrix_tmt_backoff_4D[index_search_rate][index_search_msdu_size][index_no_neighbours][index_search_likelihood_collision];
    } else if ( index_search_rate >= 0 && index_search_msdu_size >= 0 && index_no_neighbours >= 0 ) {
        //max Throughput
        //BRN_WARN("Search max tp");
        backoff_window_size = _backoff_matrix_tmt_backoff_3D[index_search_rate][index_search_msdu_size][index_no_neighbours];
    } else if ( index_search_likelihood_collision >= 0 && index_no_neighbours >= 0 ) {
      backoff_window_size = _backoff_matrix_birthday_problem_intuitv[index_search_likelihood_collision][index_no_neighbours];
    }

    if (backoff_window_size_2 > -1) backoff_window_size = backoff_window_size_2;

    if ( backoff_window_size != 0 ) {
        // Evaluate Statistics and elect a Queue
        //int index = -1; veraltet
        // Get from the Backoff-Packet-Loss-Table the supported fractions
        //for (int i = 0; i<= packet_loss_index_max; i++){ //veraltet
        //if(fraction <= _backoff_packet_loss[i]) index = i; //veraltet
        //} //veraltet
        //if (index == -1) index = packet_loss_index_max; //veraltet
        //if(number_of_neighbours > number_of_neighbours_max) number_of_neighbours = number_of_neighbours_max; // veraltet
        // Get from Backoff-Matrix-Table for the current Fraction and the current number of neighbours the backoff-value
        //unsigned int backoff_value = _backoff_matrix_birthday_problem_intuitv[fraction][number_of_neighbours];//veraltet
        //Search with the calculated backoff-value the queue for the packet  
        opt_queue = no_queues_get()-1; //init with the worst case queue 
        for (int i = 0; i <= no_queues_get()-1;i++) {
	        BRN_DEBUG("cwmin[%d] := %d",i,cwmin_get(i));
		BRN_DEBUG("cwmax[%d] := %d",i,cwmax_get(i));
            // Take the first queue, whose cw-interval is in the range of the backoff-value
            if (backoff_window_size >= cwmin_get(i) && backoff_window_size < cwmin_get(i+1)){
                opt_queue = i+1;
                break;
            }
        }
        // note the tos-value from the user, to get more packetlosses, but a higher throughput and priority
        // if tos-value is higher than opt_queue then modify opt_queue
        /*if ((tos > opt_queue) && (opt_queue < no_queues_get())) { 
            opt_queue = opt_queue + 1;
        }*/
        BRN_DEBUG("backoffwin: %d  Queue: %d", backoff_window_size, opt_queue);
    } else {
        if (tos > 0){
            if (tos == 1) {
                opt_queue = 0;
            }
            else if (tos == 2) {
                opt_queue = 2;
            }
            else if (tos == 3) {
                opt_queue = 3;
            }
        }
        else if (tos == 0) {
            opt_queue = 1;
        }
    }
    return opt_queue;
}

Packet *
Tos2QueueMapper::simple_action(Packet *p)
{
    //TOS-Value from an application or user
  	uint8_t tos = BRNPacketAnno::tos_anno(p);
  	int opt_queue = tos;

	switch (_bqs_strategy) {
		case BACKOFF_STRATEGY_OFF:
		  return p;
		case BACKOFF_STRATEGY_NEIGHBOURS_PLI_AWARE: 
		  opt_queue = backoff_strategy_neighbours_pli_aware(p,tos);

        break;
		case  BACKOFF_STRATEGY_NEIGHBOURS_CHANNEL_LOAD_AWARE: 
			if ( _colinf != NULL ) {
				if ( _colinf->_global_rs != NULL ) {
					uint32_t target_frac = tos2frac[tos];
					//find queue with min. frac of target_frac
					int ofq = -1;
					for ( int i = 0; i < no_queues; i++ ) {
						if (ofq == -1) {
							BRN_DEBUG("Foo");
							BRN_DEBUG("%p",_colinf->_global_rs);
							BRN_DEBUG("queue: %d",_colinf->_global_rs->get_frac(i));
							//int foo = _colinf->_global_rs->get_frac(i);
							if (_colinf->_global_rs->get_frac(i) >= target_frac) {
								if ( i == 0 ) {
									ofq = i;
								} else if ( _colinf->_global_rs->get_frac(i-1) == 0 ) { //TODO: ERROR-RETURN-VALUE = -1 is missing
									ofq = i - 1;
								} else {
									ofq = i;
								}
								} else {
									if ( _colinf->_global_rs->get_frac(i) >=  ( (9 * target_frac) / 10 ) ) {
										if ( i < (no_queues - 2) ) {
											ofq = 1 + 1;
										}
									}
								}
						}
					}
					if ( ofq == -1 ) opt_queue = no_queues - 1;
					else opt_queue = ofq;
				}
			} else if ( _cst != NULL ) {
				struct airtime_stats as;
				_cst->get_stats(&as,0);

				int opt_cwmin = get_cwmin(as.frac_mac_busy, as.no_sources);
				opt_queue = find_queue(opt_cwmin);

				int diff_q = (no_queues / 2) - tos - 1;
				opt_queue -= diff_q;

			
				if ( opt_queue < 0 ) opt_queue = 0;
				else if ( opt_queue > no_queues ) opt_queue = no_queues;
			}
		break;
	
  	}
  	
  	
  	//trunc overflow
  	if ( opt_queue >= no_queues ) opt_queue = no_queues - 1;

  	//set queue
 	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  	BrnWifi::setTxQueue(ceh, opt_queue);
  	//add stats
  	_queue_usage[opt_queue]++;
  	return p;
}

int
Tos2QueueMapper::get_cwmin(int /*busy*/, int /*nodes*/) {
  return 10;
}

int
Tos2QueueMapper::find_queue(int cwmin) {
  for ( int i = 0; i < no_queues; i++ ) {
    if ( _cwmin[i] > cwmin ) return i;
  }

  return no_queues - 1;
}

enum {H_TOS2QUEUEMAPPER_STATS, H_TOS2QUEUEMAPPER_RESET, H_TOS2QUEUEMAPPER_STRATEGY};

static String Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  	Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  	switch ((uintptr_t) thunk) {
    	case H_TOS2QUEUEMAPPER_STATS:
      		StringAccum sa;
      		sa << "<queueusage strategy=\"" << td->_bqs_strategy << "\" queues=\"" << (uint32_t)td->no_queues_get() << "\" >\n";
      		for ( int i = 0; i < td->no_queues_get(); i++) {
        		sa << "\t<queue index=\"" << i << "\" usage=\"" << td->queue_usage_get(i) << "\" />\n";
      		}
      		sa << "</queueusage>\n";
      		return sa.take_string();
      	break;
  	}
  return String();
}

static int Tos2QueueMapper_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  	Tos2QueueMapper *f = (Tos2QueueMapper *)e;
  	String s = cp_uncomment(in_s);
	Vector<String> args;
	cp_spacevec(s, args);

 	switch((intptr_t)vparam) {
    		case H_TOS2QUEUEMAPPER_RESET: 
      			f->reset_queue_usage();
      		        break;
    		case H_TOS2QUEUEMAPPER_STRATEGY: 
		        int st;
		        if (!cp_integer(args[0],&st)) return errh->error("strategy parameter must be integer");
			f->set_backoff_strategy(st);
		        break;
      	}
	return 0;
}

void Tos2QueueMapper::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_read_handler("queue_usage", Tos2QueueMapper_read_param, (void *) H_TOS2QUEUEMAPPER_STATS);//STATS:=statistics

  add_write_handler("reset", Tos2QueueMapper_write_param, (void *) H_TOS2QUEUEMAPPER_RESET, Handler::h_button);
  add_write_handler("strategy",Tos2QueueMapper_write_param, (void *)H_TOS2QUEUEMAPPER_STRATEGY);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapper)
ELEMENT_MT_SAFE(Tos2QueueMapper)
