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
    _bo_usage_max_no(16),
    _ac_stats_id(0),
    _target_packetloss(TOS2QM_DEFAULT_TARGET_PACKET_LOSS),
    _target_channelload(TOS2QM_DEFAULT_TARGET_CHANNELLOAD),
    _bo_for_target_channelload(TOS2QM_DEFAULT_LEARNING_BO),
    _target_diff_rxtx_busy(TOS2QM_DEFAULT_TARGET_DIFF_RXTX_BUSY),
    _feedback_cnt(0),
    _tx_cnt(0),
    _pkt_in_q(0),
    _call_set_backoff(0)
{
  BRNElement::init();
}

Tos2QueueMapper::~Tos2QueueMapper()
{
  delete[] _cwmin;
  delete[] _cwmax;
  delete[] _aifs;
  delete[] _queue_usage;
  
  delete[] _bo_exp;
  delete[] _bo_usage_usage;
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
      "TARGETPER", cpkP, cpInteger, &_target_packetloss,
      "TARGETCHANNELLOAD", cpkP, cpInteger, &_target_channelload,
      "TARGETRXTXBUSYDIFF", cpkP, cpInteger, &_target_diff_rxtx_busy,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  if ( ( _bqs_strategy != BACKOFF_STRATEGY_OFF ) &&
       ( _bqs_strategy != BACKOFF_STRATEGY_DIRECT ) &&
       ( _cst == NULL ) ) {
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

  _bo_exp = new uint16_t[no_queues];
  _bo_usage_usage = new uint32_t[_bo_usage_max_no];
  
  for ( int i = 0; i < no_queues; i++ )
    _bo_exp[i] = find_closest_backoff_exp(_cwmin[i]);    

  memset(_bo_usage_usage, 0, _bo_usage_max_no * sizeof(uint32_t));

  //for ( int i = 0; i < no_queues; i++ )
  //  click_chatter("Queue: %d Min: %d Max: %d",i ,_cwmin[i], _cwmax[i]); 
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
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  if ( EtherAddress(w->i_addr1) == brn_etheraddress_broadcast ) {
    _pkt_in_q++;
    return p;
  }

/*if ( port == 1 ) {    
      handle_feedback(p);
      return p;
    }*/
  
  //TOS-Value from an application or user
  uint8_t tos = BRNPacketAnno::tos_anno(p);
  int opt_cwmin = 15;                      //default
  int opt_queue = 0;

  switch (_bqs_strategy) {
    case BACKOFF_STRATEGY_OFF:
        switch (tos) {
          case 0: opt_queue = 1; break;
          case 1: opt_queue = 0; break;
          case 2: opt_queue = 2; break;
          case 3: opt_queue = 3; break;
          default: opt_queue = 1;
        }        
        BrnWifi::setTxQueue(ceh, opt_queue);
    case BACKOFF_STRATEGY_DIRECT:            //parts also used for BACKOFF_STRATEGY_OFF (therefore no "break;"
        _queue_usage[opt_queue]++; 
        _bo_usage_usage[_bo_exp[opt_queue]]++;
        _pkt_in_q++;
        return p;
    case BACKOFF_STRATEGY_MAX_THROUGHPUT: 
        opt_cwmin = backoff_strategy_max_throughput(p);
        break;
    case BACKOFF_STRATEGY_TARGET_PACKETLOSS:
        opt_cwmin = backoff_strategy_packetloss_aware(p);
        break;
    case BACKOFF_STRATEGY_LEARNING:
        opt_cwmin = find_queue(_learning_current_bo);
        break;  
    case BACKOFF_STRATEGY_TARGET_DIFF_RXTX_BUSY:
    case BACKOFF_STRATEGY_CHANNEL_LOAD_AWARE:
    {
        struct airtime_stats *as = _cst->get_latest_stats();

        opt_cwmin = _bo_for_target_channelload; //also used for diff_rxtx_busy
        if ( _ac_stats_id != as->stats_id ) {
          if ( _bqs_strategy == BACKOFF_STRATEGY_CHANNEL_LOAD_AWARE ) {
            opt_cwmin = backoff_strategy_channelload_aware(as->hw_busy, as->no_sources);
          } else {
            opt_cwmin = backoff_strategy_rxtx_busy_diff_aware(as->hw_rx, as->hw_tx, as->hw_busy, as->no_sources);
          }
          _ac_stats_id = as->stats_id;
        }
        break;
    }
      
  }

  opt_queue = find_queue(opt_cwmin);

  //trunc overflow
  if ( opt_queue == (no_queues-1) || opt_queue == 0 ) {
    recalc_backoff_queues(find_closest_backoff(opt_cwmin), 1 /*tos*/, 1/*step*/);
    set_backoff();
  }
  
  //Apply tos;
 
  //set queue
  BrnWifi::setTxQueue(ceh, opt_queue);
 
  //add stats
  _queue_usage[opt_queue]++;
  _bo_usage_usage[_bo_exp[opt_queue]]++;
 
  _pkt_in_q++;
  return p;
}

/*
void
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

    _pkt_in_q--;
   
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
      _learning_current_bo = _learning_current_bo;       //keep
    } else if ( ceh->retries > _learning_bo_limit && _learning_current_bo < _learning_bo_max ) {
      _learning_count_up += 1;
      _learning_current_bo = _learning_current_bo << 1;
      //_learning_count_up += (ceh->retries - _learning_bo_limit);
      //_learning_current_bo = _learning_current_bo << (ceh->retries - _learning_bo_limit);
    }
  }
}

int
Tos2QueueMapper::backoff_strategy_packetloss_aware(Packet *p)
{
  int32_t number_of_neighbours = 1;
  int32_t index_search_rate = -1;
  int32_t index_search_msdu_size = -1;
  int32_t index_search_likelihood_collision = -1;
  int32_t index_no_neighbours = -1;
  uint32_t backoff_window_size = 0;

  // Get Destination Address from the current packet
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  //Get Number of Neighbours from the Channelstats-Element (_cst)
  struct airtime_stats *as = _cst->get_latest_stats(); //get airtime statisics
  number_of_neighbours = as->no_sources;
  BRN_DEBUG("Number of Neighbours %d", number_of_neighbours);

  index_search_rate = find_closest_rate_index(ceh->rate);
  index_search_msdu_size = find_closest_size_index(p->length());
  index_search_likelihood_collision = find_closest_per_index(_target_packetloss);
  index_no_neighbours = find_closest_no_neighbour_index(number_of_neighbours);

  // Tests what is known
  backoff_window_size = _backoff_matrix_tmt_backoff_4D[index_search_rate][index_search_msdu_size][index_no_neighbours][index_search_likelihood_collision];

  BRN_DEBUG("backoffwin: %d  Queue: %d", backoff_window_size, find_queue(backoff_window_size));
  
  return backoff_window_size;
}

int
Tos2QueueMapper::backoff_strategy_max_throughput(Packet *p)
{
  int32_t number_of_neighbours = 1;
  int32_t index_search_rate = -1;
  int32_t index_search_msdu_size = -1;
  int32_t index_no_neighbours = -1;
  uint32_t backoff_window_size = 0;

  // Get Destination Address from the current packet
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  //Get Number of Neighbours from the Channelstats-Element (_cst)
  struct airtime_stats *as = _cst->get_latest_stats(); //get airtime statisics
  number_of_neighbours = as->no_sources;
  BRN_DEBUG("Number of Neighbours %d", number_of_neighbours);

  index_search_rate = find_closest_rate_index(ceh->rate);
  index_search_msdu_size = find_closest_size_index(p->length());
  index_no_neighbours = find_closest_no_neighbour_index(number_of_neighbours);

  //max Throughput
  //BRN_WARN("Search max tp");
  backoff_window_size = _backoff_matrix_tmt_backoff_3D[index_search_rate][index_search_msdu_size][index_no_neighbours];

  BRN_DEBUG("backoffwin: %d  Queue: %d", backoff_window_size, find_queue(backoff_window_size));
  
  return backoff_window_size;
}

int
Tos2QueueMapper::backoff_strategy_channelload_aware(int busy, int /*nodes*/)
{
  BRN_DEBUG("BUSY: %d _traget_channel: %d bo: %d", busy, _target_channelload,_bo_for_target_channelload);
  
  if ( (busy < ((int32_t)_target_channelload - 5)) && ( (int32_t)_bo_for_target_channelload > 1 ) ) {
    _bo_for_target_channelload = _bo_for_target_channelload >> 1;
  } else if ( (busy > ((int32_t)_target_channelload + 5)) && ((int32_t)_bo_for_target_channelload < (int32_t)_learning_max_bo)) {
    _bo_for_target_channelload = _bo_for_target_channelload << 1;
  }
  
  BRN_DEBUG("bo: %d", _bo_for_target_channelload);
  
  return _bo_for_target_channelload;
}

int
Tos2QueueMapper::backoff_strategy_rxtx_busy_diff_aware(int rx, int tx, int busy, int /*nodes*/)
{
  int diff = busy-(tx+rx);
  BRN_DEBUG("REG: %d %d %d -> %d _traget_diff: %d bo: %d",rx, tx, busy, diff, _target_diff_rxtx_busy,_bo_for_target_channelload);
  
  if ( (diff < ((int32_t)_target_diff_rxtx_busy - 3)) && ( (int32_t)_bo_for_target_channelload > 1 ) ) {
    _bo_for_target_channelload = _bo_for_target_channelload >> 1;
  } else if ( (diff > ((int32_t)_target_diff_rxtx_busy + 3)) && ((int32_t)_bo_for_target_channelload < (int32_t)_learning_max_bo)) {
    _bo_for_target_channelload = _bo_for_target_channelload << 1;
  }
  
  BRN_DEBUG("bo: %d", _bo_for_target_channelload);
  
  return _bo_for_target_channelload;
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


/* TODO: use gravitaion approach:
 *       -calc dist to both neighbouring backoffs
 *       -calc gravitaion (f = C/d*d)
 *       -get random r between 0 & (f1+f2) 
 *       -choose 1 if 0 < r < f1
 *       -choose 2 if f1 < r < (f1+f2)
 * 
 * Use also for find_closest_backoff(_exp) ?
 */
int
Tos2QueueMapper::find_queue(uint16_t backoff_window_size)
{
  if ( backoff_window_size <= _cwmin[0] ) return 0;

  // Take the first queue, whose cw-interval is in the range of the backoff-value
  for (int i = 0; i <= no_queues-1; i++)
    if ( backoff_window_size > _cwmin[i] && backoff_window_size <= _cwmin[i+1] ) return i+1;

  return no_queues-1;
}

int
Tos2QueueMapper::find_queue_prob(uint16_t backoff_window_size)
{
  if ( backoff_window_size <= _cwmin[0] ) return 0;

  // Take the first queue, whose cw-interval is in the range of the backoff-value
  for (int i = 0; i <= no_queues-1; i++)
    if ( backoff_window_size > _cwmin[i] && backoff_window_size <= _cwmin[i+1] ) {
      if (backoff_window_size <= _cwmin[i+1]) return i+1;

      int dist_lower_queue = 1000000000 / ((uint32_t)backoff_window_size - (uint32_t)_cwmin[i]);
      int dist_upper_queue = 1000000000 / ((uint32_t)_cwmin[i+1] - (uint32_t)backoff_window_size);

      if ( (click_random() % (dist_lower_queue + dist_upper_queue)) < (int32_t)dist_lower_queue ) return i;
      
      return i+1;
    }

  return no_queues-1;
}

uint32_t
Tos2QueueMapper::find_closest_backoff(uint32_t bo)
{
  uint32_t c_bo = 1;
  
  while ( c_bo < bo ) c_bo = c_bo << 1;
  return c_bo;
}

uint32_t
Tos2QueueMapper::find_closest_backoff_exp(uint32_t bo)
{
  uint32_t c_bo_exp = 1;
  
  while ( (uint32_t)(1 << c_bo_exp) < bo ) c_bo_exp++;
  return c_bo_exp;
}

uint32_t
Tos2QueueMapper::get_queue_usage(uint8_t position)
{
  if (position >= no_queues) position = no_queues - 1;
  return _queue_usage[position];
}

uint32_t
Tos2QueueMapper::recalc_backoff_queues(uint32_t backoff, uint32_t tos, uint32_t step)
{
#if CLICK_NS
  uint32_t cwmin = backoff << (tos*step);
  if ( cwmin <= 1 ) cwmin = 2;
  
  for ( uint32_t i = 0; i < no_queues; i++, cwmin = cwmin << step) {
    _cwmin[i] = cwmin - 1;
    _cwmax[i] = (cwmin << 6) - 1;
    _bo_exp[i] = MIN(_bo_usage_max_no-1, find_closest_backoff_exp(_cwmin[i]));
    BRN_DEBUG("Queue %d: %d %d",i,_cwmin[i],_cwmax[i]); 
  }
#else
  BRN_DEBUG("Try to set queues BO: %d TOS: %d STEP: %d",backoff ,tos, step); 
#endif
  return 0;
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
  
  _call_set_backoff++;
#endif
  return 0;
}

/****************************************************************************************************************/
/*********************************** H A N D L E R **************************************************************/
/****************************************************************************************************************/

enum {H_TOS2QUEUEMAPPER_STATS, H_TOS2QUEUEMAPPER_RESET, H_TOS2QUEUEMAPPER_STRATEGY};

String
Tos2QueueMapper::stats()
{
  StringAccum sa;
  sa << "<tos2queuemapper node=\"" << BRN_NODE_NAME << "\" strategy=\"" << _bqs_strategy << "\" queues=\"";
  sa << (uint32_t)no_queues << "\" learning_current_bo=\"" << _learning_current_bo;
  sa << "\" bo_up=\"" << _learning_count_up << "\" bo_down=\"" << _learning_count_down;
  sa << "\" bo_tcl=\"" << _bo_for_target_channelload << "\" tar_cl=\"" << _target_channelload;
  sa << "\" feedback_cnt=\"" << _feedback_cnt << "\" tx_cnt=\"" << _tx_cnt;
  sa << "\" packets_in_queue=\"" << _pkt_in_q << "\" calls_set_backoff=\"" << _call_set_backoff << "\" >\n";
  sa << "\t<queueusage>\n";
  for ( int i = 0; i < no_queues; i++) {
    sa << "\t\t<queue index=\"" << i << "\" usage=\"" << _queue_usage[i];
    sa << "\" cwmin=\"" << _cwmin[i] << "\" cwmax=\"" << _cwmax[i];
    sa << "\" bo_exp=\"" << _bo_exp[i] << "\" />\n";
  }
  sa << "\t</queueusage>\n";
  sa << "\t<backoffusage>\n";
  for ( uint32_t i = 0; i < _bo_usage_max_no; i++) {
    sa << "\t\t<backoff value=\"" << (uint32_t)((uint32_t)1 << i)-1  << "\" usage=\"" << _bo_usage_usage[i];
    sa << "\" exp=\"" << i << "\" />\n";
  }
  sa << "\t</backoffusage>\n</tos2queuemapper>\n";
  return sa.take_string();
}

static String Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  switch ((uintptr_t) thunk) {
    case H_TOS2QUEUEMAPPER_STATS:
      return td->stats();
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
