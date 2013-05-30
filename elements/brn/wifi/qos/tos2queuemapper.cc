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
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#if CLICK_NS 
#include <click/router.hh>
#endif 

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "tos2queuemapper.hh"

CLICK_DECLS

#include "tos2queuemapper_data.hh"

Tos2QueueMapper::Tos2QueueMapper():
    _bqs_strategy(BACKOFF_STRATEGY_OFF),
    _cst(NULL),     //Channelstats-Element
    _colinf(NULL),  //Collission-Information-Element
    _learning_current_bo(TOS2QM_DEFAULT_LEARNING_BO),
    _learning_count_up(0),
    _learning_count_down(0),
    _learning_max_bo(0),
    _feedback_cnt(0),
    _tx_cnt(0)

{
  BRNElement::init();
  _likelihood_collison = -1;
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
  uint32_t v;

  if (cp_va_kparse(conf, this, errh,
      "CWMIN", cpkP, cpString, &s_cwmin,
      "CWMAX", cpkP, cpString, &s_cwmax,
      "AIFS", cpkP, cpString, &s_aifs,
      "STRATEGY", cpkP, cpInteger, &_bqs_strategy,
      "CHANNELSTATS", cpkP, cpElement, &_cst,
      "COLLISIONINFO", cpkP, cpElement, &_colinf,
      "LIKELIHOODCOLLISON", cpkP, cpInteger, &_likelihood_collison,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  if ( ( _bqs_strategy != BACKOFF_STRATEGY_OFF )  && ( _cst == NULL ) ) {
    BRN_WARN("Channelstats is NULL! Turn of TOS2QM-Strategy.");
    _bqs_strategy = BACKOFF_STRATEGY_OFF;
  }
  
  Vector<String> args;
  cp_spacevec(s_cwmin, args);
  no_queues = args.size();
  if ( no_queues > 0 ) {
    _cwmin = new uint16_t[no_queues];
    _cwmax = new uint16_t[no_queues];
    _aifs = new uint16_t[no_queues];

    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmin[i] = v;
      if ( v > _learning_max_bo ) _learning_max_bo = v;
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
  
#if CLICK_NS  
  get_backoff();
#endif

  _queue_usage = new uint32_t[no_queues];
  reset_queue_usage();

  for ( int i = 0; i < no_queues; i++ )
    click_chatter("Queue: %d Min: %d Max: %d",i ,_cwmin[i], _cwmax[i]); 
  //for ( int i = 0; i < 25; i++) {
  //  BRN_ERROR("N: %d backoff: %d",i,_backoff_matrix_tmt_backoff_3D[0 /*rate 1*/][1/*msdu 1500*/][i]);//
  //}
  return 0;
}

/*Packet *
Tos2QueueMapper::smaction(Packet *p, int port)
*/
Packet *
Tos2QueueMapper::simple_action(Packet *p)
{
  struct click_wifi *w = (struct click_wifi *) p->data();
  if ( EtherAddress(w->i_addr1) == brn_etheraddress_broadcast ) return p;
 
  /*  if ( port == 1 ) {
    
    handle_feedback(p);
    return p;
  }
*/  
  //TOS-Value from an application or user
  uint8_t tos = BRNPacketAnno::tos_anno(p);
  int opt_queue = tos;

  switch (_bqs_strategy) {
    case BACKOFF_STRATEGY_OFF:
        return p;
/*    case BACKOFF_STRATEGY_MAX_THROUGHPUT: 
        opt_queue = backoff_strategy_max_throughput(p, tos);
        break;*/
    case  BACKOFF_STRATEGY_CHANNEL_LOAD_AWARE: 
        if ( _colinf != NULL ) {
          if ( _colinf->_global_rs != NULL ) {
        
            uint32_t target_frac = tos2frac[tos];
        
            //find queue with min. frac of target_frac
            int ofq = -1;
            for ( int i = 0; i < no_queues; i++ ) {
              if (ofq == -1) {
                BRN_DEBUG("%p",_colinf->_global_rs);
                BRN_DEBUG("queue: %d",_colinf->_global_rs->get_frac(i));

                if (_colinf->_global_rs->get_frac(i) >= target_frac) {
                  if ( i == 0 ) ofq = i;
                  else if ( _colinf->_global_rs->get_frac(i-1) == 0 )  ofq = i - 1;  //TODO: ERROR-RETURN-VALUE = -1 is missing
                  else ofq = i;
                } else {
                  if ( _colinf->_global_rs->get_frac(i) >=  ( (9 * target_frac) / 10 ) ) {
                    if ( i < (no_queues - 2) ) ofq = 1 + 1;
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

          int opt_cwmin = backoff_strategy_channelload_aware(as.frac_mac_busy, as.no_sources);
          opt_queue = find_queue(opt_cwmin);

          int diff_q = (no_queues / 2) - tos - 1;
          opt_queue -= diff_q;

          if ( opt_queue < 0 ) opt_queue = 0;
          else if ( opt_queue > no_queues ) opt_queue = no_queues;
        }
        break;
    case BACKOFF_STRATEGY_LEARNING:
        opt_queue = find_queue(_learning_current_bo);
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

/*void
Tos2QueueMapper::push(int port, Packet *p)
{
  if (Packet *q = smaction(p, port)) {
    output(port).push(q);
  }
}

Packet *
Tos2QueueMapper::pull(int port)
{
  if (Packet *p = input(port).pull()) {
    return smaction(p, port);
  } else {
    return 0;
  }
}
*/
void
Tos2QueueMapper::handle_feedback(Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  if ( ceh->flags & WIFI_EXTRA_TX ) {
   
    struct click_wifi *w = (struct click_wifi *) p->data();
    if ( EtherAddress(w->i_addr1) == brn_etheraddress_broadcast ) return;

    //BRN_FATAL("Feedback: %d Dst: %s", ceh->retries, EtherAddress(w->i_addr1).unparse().c_str());

    _feedback_cnt++;
    _tx_cnt += ceh->retries + 1;

    uint32_t _learning_bo_limit = 1;
    
    uint32_t _learning_bo_min = _cwmin[0];           //1
    uint32_t _learning_bo_max = _cwmin[no_queues-1]; //_learning_max_bo
    
    
    if ( ceh->retries < _learning_bo_limit && _learning_current_bo > _learning_bo_min ) { //no retries: reduces bo
      _learning_count_down++;
      _learning_current_bo = _learning_current_bo >> 1;
    } else if ( ceh->retries == _learning_bo_limit ) {
      //_learning_count_down++;
      //_learning_current_bo = _learning_current_bo >> 1;
      _learning_current_bo = _learning_current_bo; //keep
    } else if ( ceh->retries > _learning_bo_limit && _learning_current_bo < _learning_bo_max ) {
      _learning_count_up += 1;
      _learning_current_bo = _learning_current_bo << 1;
      //_learning_count_up += (ceh->retries - _learning_bo_limit);
      //_learning_current_bo = _learning_current_bo << (ceh->retries - _learning_bo_limit);
    }
  }
}

int
Tos2QueueMapper::backoff_strategy_neighbours_aware(Packet *p, uint8_t tos)
{
    int32_t number_of_neighbours = 1;
    int32_t  index_search_rate = -1;
    int32_t  index_search_msdu_size = -1;
    int32_t  index_search_likelihood_collision = -1;
    int32_t  index_no_neighbours = -1;
    uint32_t  backoff_window_size = 0;
    int backoff_window_size_2 = -1;
    int opt_queue = -1;

    // Get Destination Address from the current packet
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

    //Get Number of Neighbours from the Channelstats-Element (_cst)
    if ( NULL != _cst ) {
        struct airtime_stats *as;
	as = _cst->get_latest_stats(); // _cst->get_stats(&as,0);//get airtime statisics
        number_of_neighbours = as->no_sources;
        BRN_DEBUG("Number of Neighbours %d", number_of_neighbours);  
    }

    index_search_rate = find_closest_rate_index(ceh->rate);
    index_search_msdu_size = find_closest_size_index(p->length());
    index_search_likelihood_collision = find_closest_per_index(_likelihood_collison);
    index_no_neighbours = find_closest_no_neighbour_index(number_of_neighbours);

    // Tests what is known
    if ( index_search_rate >= 0 && index_search_msdu_size >= 0 && index_search_likelihood_collision >= 0 && index_no_neighbours >= 0 ) {
       backoff_window_size_2 = _backoff_matrix_tmt_backoff_4D[index_search_rate][index_search_msdu_size][index_no_neighbours][index_search_likelihood_collision];
    } else if ( index_search_rate >= 0 && index_search_msdu_size >= 0 && index_no_neighbours >= 0 ) {
        //max Throughput
        //BRN_WARN("Search max tp");
        backoff_window_size = _backoff_matrix_tmt_backoff_3D[index_search_rate][index_search_msdu_size][index_no_neighbours];
    }

    if (backoff_window_size_2 > -1) backoff_window_size = backoff_window_size_2;

    if ( backoff_window_size != 0 ) {
        opt_queue = get_no_queues()-1; //init with the worst case queue 
        for (int i = 0; i <= get_no_queues()-1;i++) {
	        BRN_DEBUG("cwmin[%d] := %d",i,get_cwmin(i));
		BRN_DEBUG("cwmax[%d] := %d",i,get_cwmax(i));
            // Take the first queue, whose cw-interval is in the range of the backoff-value
            if (backoff_window_size >= get_cwmin(i) && backoff_window_size < get_cwmin(i+1)){
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
    switch (tos) {
      case 0: opt_queue = 1; break;
      case 1: opt_queue = 0; break;
      case 2: opt_queue = 2; break;
      case 3: opt_queue = 3; break;
      default: opt_queue = 1;
    }
  }
  
  return opt_queue;
}

int
Tos2QueueMapper::backoff_strategy_channelload_aware(int /*busy*/, int /*nodes*/)
{
  return 10;
}


/**
 *   H E L P E R   F U N C T I O N S 
 */

int
Tos2QueueMapper::find_closest_size_index(int size)
{
  for ( int i = 0; i < T2QM_MAX_INDEX_MSDU_SIZE; i++ )
    if ( vector_msdu_sizes[i] >= size ) return i;
  
  return T2QM_MAX_INDEX_MSDU_SIZE - 1;
}

int
Tos2QueueMapper::find_closest_rate_index(int rate)
{
  for ( int i = 0; i < T2QM_MAX_INDEX_RATES; i++ )
    if ( vector_rates_data[i] >= rate ) return i;
  
  return T2QM_MAX_INDEX_RATES - 1;
}

int
Tos2QueueMapper::find_closest_no_neighbour_index(int no_neighbours) {
  if ( no_neighbours > T2QM_MAX_INDEX_NO_NEIGHBORS ) return T2QM_MAX_INDEX_NO_NEIGHBORS-1;
  return no_neighbours - 1;
}

int
Tos2QueueMapper::find_closest_per_index(int per) {
  for ( int i = 0; i < T2QM_MAX_INDEX_PACKET_LOSS; i++ )
    if ( _backoff_packet_loss[i] >= per ) return i;
  
  return T2QM_MAX_INDEX_PACKET_LOSS - 1;
}

int
Tos2QueueMapper::find_queue(int cwmin) {
  for ( int i = 0; i < no_queues; i++ )
    if ( _cwmin[i] > cwmin ) return i;
  
  return no_queues - 1;
}

uint32_t
Tos2QueueMapper::get_queue_usage(uint8_t position)
{
  if (position >= no_queues) position = no_queues - 1;
  return _queue_usage[position];
}

uint32_t
Tos2QueueMapper::get_cwmin(uint8_t position)
{
  if (position >= no_queues) position = no_queues -1;
  return _cwmin[position];
}

uint32_t
Tos2QueueMapper::get_cwmax(uint8_t position)
{
  if (position >= no_queues) position = no_queues - 1;
  return _cwmax[position];
}

uint32_t
Tos2QueueMapper::set_backoff()
{
#if CLICK_NS
  uint32_t *queueinfo = new uint32_t[1 + 2 * no_queues];
  queueinfo[0] = no_queues;
  for ( int q = 0; q < no_queues; q++ ) {
    queueinfo[1 + q] = _cwmin[q];
    queueinfo[1 + no_queues + q] = _cwmax[q];
  }
  
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_SET_BACKOFF, queueinfo);
  
  delete[] queueinfo;
#endif
  return 0;
}

uint32_t
Tos2QueueMapper::get_backoff()
{
#if CLICK_NS
  uint32_t *queueinfo = new uint32_t[1 + 2 * no_queues];
  queueinfo[0] = no_queues;
  
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_GET_BACKOFF, queueinfo);

  int max_q = no_queues;
  if ( queueinfo[0] < no_queues ) max_q = queueinfo[0];
  
  for ( int q = 0; q < max_q; q++ ) {
    _cwmin[q] = queueinfo[1 + q];
    _cwmax[q] = queueinfo[1 + max_q + q];
  }
    
  delete[] queueinfo;
#endif
  return 0;
}

/****************************************************************************************************************/
/*********************************** H A N D L E R **************************************************************/
/****************************************************************************************************************/

enum {H_TOS2QUEUEMAPPER_STATS, H_TOS2QUEUEMAPPER_RESET, H_TOS2QUEUEMAPPER_STRATEGY};

static String Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  	Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  	switch ((uintptr_t) thunk) {
    	case H_TOS2QUEUEMAPPER_STATS:
      		StringAccum sa;
                sa << "<tos2queuemapper strategy=\"" << td->_bqs_strategy << "\" queues=\"" << (uint32_t)td->get_no_queues();
                sa << "\" learning_current_bo=\"" << td->get_learning_current_bo();
                sa << "\" bo_up=\"" << td->get_learning_count_up() << "\" bo_down=\"" << td->get_learning_count_down();
                sa << "\" feedback_cnt=\"" << td->_feedback_cnt << "\" tx_cnt=\"" << td->_tx_cnt << "\" >\n";
      		sa << "\t<queueusage>\n";
      		for ( int i = 0; i < td->get_no_queues(); i++) {
        		sa << "\t\t<queue index=\"" << i << "\" usage=\"" << td->get_queue_usage(i) << "\" />\n";
      		}
      		sa << "\t</queueusage>\n</tos2queuemapper>\n";
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

  add_read_handler("stats", Tos2QueueMapper_read_param, (void *) H_TOS2QUEUEMAPPER_STATS);//STATS:=statistics

  add_write_handler("reset", Tos2QueueMapper_write_param, (void *) H_TOS2QUEUEMAPPER_RESET, Handler::h_button);
  add_write_handler("strategy",Tos2QueueMapper_write_param, (void *)H_TOS2QUEUEMAPPER_STRATEGY);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapper)
ELEMENT_MT_SAFE(Tos2QueueMapper)
