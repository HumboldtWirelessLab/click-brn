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
#include "bo_schemes/tos2qm_data.hh"


CLICK_DECLS


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
    _call_set_backoff(0),
    _current_scheme(0),
    _bo_schemes(0),
    _no_schemes(0),
    _scheme_id(-1)
{
  BRNElement::init();
}

Tos2QueueMapper::~Tos2QueueMapper()
{
  delete[] _bo_schemes;
  delete[] _bo_exp;
  delete[] _bo_usage_usage;
  delete[] _queue_usage;
  delete[] _cwmin;
  delete[] _cwmax;
  delete[] _aifs;
}

int
Tos2QueueMapper::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String s_cwmin = "";
  String s_cwmax = "";
  String s_aifs = "";
  String s_schemes = "";

  if (cp_va_kparse(conf, this, errh,
      "CWMIN", cpkP, cpString, &s_cwmin,
      "CWMAX", cpkP, cpString, &s_cwmax,
      "AIFS", cpkP, cpString, &s_aifs,
      "STRATEGY", cpkP, cpInteger, &_bqs_strategy,
      "COLLISIONINFO", cpkP, cpElement, &_colinf,
      "BO_SCHEMES", cpkP, cpString, &s_schemes,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  parse_queues(s_cwmin, s_cwmax, s_aifs);

  init_stats();

  BRN_DEBUG("");
  for (uint8_t i = 0; i < no_queues; i++)
    BRN_DEBUG("Tos2QM.configure(): Q %d: %d %d\n", i, _cwmin[i], _cwmax[i]);

#if CLICK_NS
  get_backoff();
#endif

  if (s_schemes != "")
    parse_bo_schemes(s_schemes, errh);

  return 0;
}

void
Tos2QueueMapper::init_stats()
{
  _queue_usage = new uint32_t[no_queues];
  reset_queue_usage();

  _bo_exp = new uint16_t[no_queues];
  _bo_usage_usage = new uint32_t[_bo_usage_max_no];

  for ( int i = 0; i < no_queues; i++ )
    _bo_exp[i] = find_closest_backoff_exp(_cwmin[i]);

  memset(_bo_usage_usage, 0, _bo_usage_max_no * sizeof(uint32_t));
}

void
Tos2QueueMapper::parse_queues(String s_cwmin, String s_cwmax, String s_aifs)
{
  uint32_t v;
  Vector<String> args;

  cp_spacevec(s_cwmin, args);
  no_queues = args.size();

  if ( no_queues > 0 ) {
    _cwmin = new uint16_t[no_queues];
    _cwmax = new uint16_t[no_queues];
    _aifs  = new uint16_t[no_queues];

    for( uint8_t i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmin[i] = v;
      if ( v > _learning_max_bo ) _learning_max_bo = v;
    }
    args.clear();

    cp_spacevec(s_cwmax, args);
    if ( args.size() < no_queues )
      no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmax[i] = v;
    }
    args.clear();

    cp_spacevec(s_aifs, args);
    if ( args.size() < no_queues )
      no_queues = args.size();
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

  for ( uint8_t i = 0; i < no_queues; i++ )
    _bo_exp[i] = find_closest_backoff_exp(_cwmin[i]);

  memset(_bo_usage_usage, 0, _bo_usage_max_no * sizeof(uint32_t));
}

int
Tos2QueueMapper::parse_bo_schemes(String s_schemes, ErrorHandler* errh)
{
  Vector<String> schemes;

  String s_schemes_uncomment = cp_uncomment(s_schemes);
  cp_spacevec(s_schemes_uncomment, schemes);

  _no_schemes = schemes.size();

  if (_no_schemes == 0) {
    BRN_DEBUG("Tos2QM.parse_bo_schemes(): No backoff schemes were given! STRAT = %d\n", _bqs_strategy);
    _current_scheme = NULL;
    return 0;
  }

  _bo_schemes = new BackoffScheme*[_no_schemes];

  for (uint16_t i = 0; i < _no_schemes; i++) {
    Element *e = cp_element(schemes[i], this, errh);
    BackoffScheme *bo_scheme = (BackoffScheme *) e->cast("BackoffScheme");

    if (!bo_scheme) {
      return errh->error("Element is not a 'BackoffScheme'");
    } else {
      _bo_schemes[i] = bo_scheme;
      _bo_schemes[i]->set_conf(_cwmin[0], _cwmin[no_queues - 1]);
    }
  }

  BRN_DEBUG("Tos2QM.parse_bo_schemes(): strat %d no_schemes %d\n", _bqs_strategy, _no_schemes);

  _current_scheme = get_bo_scheme(_bqs_strategy); // get responsible scheme

  if ( _current_scheme != NULL ) {
    _current_scheme->set_strategy(_bqs_strategy); // set final strategy on that scheme
  }

  return 0;
}

BackoffScheme *Tos2QueueMapper::get_bo_scheme(uint16_t strategy)
{
  for (uint16_t i = 0; i < _no_schemes; i++) {
    if (_bo_schemes[i]->handle_strategy(strategy)) {
      return _bo_schemes[i];
    }
  }

  return NULL;
}


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

  // TOS-Value from an application or user
  uint8_t tos = BRNPacketAnno::tos_anno(p);
  int opt_cwmin = 31; //default
  int opt_queue = tos;

  if (_current_scheme) {
    opt_cwmin = _current_scheme->get_cwmin(p, tos);
  } else {
    switch (_bqs_strategy) {
      case BACKOFF_STRATEGY_OFF:
          switch (tos) {
            case 0: opt_queue  = 1; break;
            case 1: opt_queue  = 0; break;
            case 2: opt_queue  = 2; break;
            case 3: opt_queue  = 3; break;
            default: opt_queue = 1;
          }
          BrnWifi::setTxQueue(ceh, opt_queue);
      case BACKOFF_STRATEGY_DIRECT:            // parts also used for BACKOFF_STRATEGY_OFF (therefore no "break;"
          _queue_usage[opt_queue]++;
          _bo_usage_usage[_bo_exp[opt_queue]]++;
          _pkt_in_q++;

          return p;
    }
  }

  opt_queue = find_queue(opt_cwmin);

  // handle trunc overflow
  if ( opt_queue == (no_queues-1) || opt_queue == 0 ) {
    uint32_t closest_bo = find_closest_backoff(opt_cwmin);
    recalc_backoff_queues(closest_bo, 1, 1);
    set_backoff();
    opt_queue = find_queue(opt_cwmin); // queues changed, find opt queue again
  }

  // Apply tos;
  // TODO ...

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
  if (!(ceh->flags & WIFI_EXTRA_TX))
    return;

  struct click_wifi *wh = (struct click_wifi *) p->data();
  if (EtherAddress(wh->i_addr1) == brn_etheraddress_broadcast)
    return;

  _pkt_in_q--;
  _feedback_cnt++;
  _tx_cnt += ceh->retries + 1;

  if (_current_scheme)
    _current_scheme->handle_feedback(ceh->retries);
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

      if ( (click_random() % (dist_lower_queue + dist_upper_queue)) < (uint32_t)dist_lower_queue ) return i;

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
  uint32_t cwmin = backoff >> (tos*step);
  if ( cwmin <= 1 ) cwmin = 2;

  for ( uint32_t i = 0; i < no_queues; i++, cwmin = cwmin << step) {
    _cwmin[i] = cwmin - 1;
    _cwmax[i] = (cwmin << 6) - 1;
    _bo_exp[i] = MIN(_bo_usage_max_no-1, find_closest_backoff_exp(_cwmin[i]));
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

  _call_set_backoff++;
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
