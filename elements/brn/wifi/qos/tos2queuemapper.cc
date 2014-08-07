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

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/wifi/brnwifi.hh"

#include "tos2queuemapper.hh"
#include "bo_schemes/tos2qm_data.hh"


CLICK_DECLS


Tos2QueueMapper::Tos2QueueMapper():
    _current_scheme(NULL),
    _bqs_strategy(BACKOFF_STRATEGY_OFF),
    _mac_bo_scheme(MAC_BACKOFF_SCHEME_DEFAULT),
    _queue_mapping(QUEUEMAPPING_DEFAULT),
    _qm_diff_queue_mode(QUEUEMAPPING_DIFFQUEUE_DEFAULT),
    _qm_diff_queue_val(1),
    _qm_diff_minmaxcw_mode(QUEUEMAPPING_DIFF_MINMAXCW_DEFAULT),
    _qm_diff_minmaxcw_val(1),
    _bo_usage_max_no(16),
    _last_bo_usage(NULL),
    _all_bos(NULL),
    _all_bos_idx(0),
    _ac_stats_id(0),
    _feedback_cnt(0),
    _tx_cnt(0),
    _pkt_in_q(0),
    _call_set_backoff(0)
{
  BRNElement::init();
  _scheme_list = SchemeList("BackoffScheme");
}

Tos2QueueMapper::~Tos2QueueMapper()
{
  delete[] _bo_exp;
  delete[] _bo_usage_usage;
  delete[] _last_bo_usage;
  delete[] _queue_usage;
  delete[] _all_bos;
}

int
Tos2QueueMapper::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String s_schemes = "";

  if (cp_va_kparse(conf, this, errh,
      "DEVICE", cpkP+cpkM, cpElement, &_device,
      "STRATEGY", cpkP, cpInteger, &_bqs_strategy,
      "BO_SCHEMES", cpkP, cpString, &s_schemes,
      "MAC_BO_SCHEME", cpkP, cpInteger, &_mac_bo_scheme,
      "QUEUEMAPPING", cpkP, cpInteger, &_queue_mapping,
      "QUEUEMODE", cpkP, cpInteger, &_qm_diff_queue_mode,
      "QUEUEVAL", cpkP, cpInteger, &_qm_diff_queue_val,
      "CWMINMAXMODE", cpkP, cpInteger, &_qm_diff_minmaxcw_mode,
      "CWMINMAXVAL", cpkP, cpInteger, &_qm_diff_minmaxcw_val,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  _scheme_list.set_scheme_string(s_schemes);

  return 0;
}

void
Tos2QueueMapper::init_stats()
{
  _queue_usage = new uint32_t[no_queues];
  reset_queue_usage();

  _bo_exp = new uint16_t[no_queues];

  /* "translates" queue to exponent, e.g. cwmin[0] = 32 -> _bo_exp[0] = 6 since 2⁶ = 32 */
  for ( int i = 0; i < no_queues; i++ )
    _bo_exp[i] = find_closest_backoff_exp(_cwmin[i]);

  _bo_usage_usage = new uint32_t[_bo_usage_max_no];
  memset(_bo_usage_usage, 0, _bo_usage_max_no * sizeof(uint32_t));

  _last_bo_usage = new Timestamp[_bo_usage_max_no];
  memset(_last_bo_usage, 0, _bo_usage_max_no * sizeof(Timestamp));

  if (TOS2QM_ALL_BOS_STATS) {
    _all_bos = new int16_t[TOS2QM_BOBUF_SIZE];
    memset(_all_bos, -1, TOS2QM_BOBUF_SIZE * sizeof(uint16_t));
  }
}

int
Tos2QueueMapper::initialize (ErrorHandler *errh)
{
  click_brn_srandom();

  _scheme_list.parse_schemes(this, errh);
  set_backoff_strategy(_bqs_strategy);
  set_mac_backoff_scheme(_mac_bo_scheme);

  get_backoff();
  if (BRN_DEBUG_LEVEL_DEBUG) print_queues();

  init_stats();

  return 0;
}

BackoffScheme *
Tos2QueueMapper::get_bo_scheme(uint32_t strategy)
{
  return (BackoffScheme *)_scheme_list.get_scheme(strategy);
}

void
Tos2QueueMapper::set_backoff_strategy(uint32_t strategy)
{
  _bqs_strategy = strategy;

  _current_scheme = get_bo_scheme(_bqs_strategy);

  if ( _current_scheme ) {
    _current_scheme->set_conf(BACKOFF_SCHEME_MIN_CWMIN, BACKOFF_SCHEME_MAX_CWMAX);
    _current_scheme->set_strategy(_bqs_strategy);
  }
}

Packet *
Tos2QueueMapper::simple_action(Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  // TOS-Value from an application or user
  uint8_t tos = BRNPacketAnno::tos_anno(p);
  int opt_cwmin = 31; //default
  int opt_queue = tos;

  if (_current_scheme) {
    opt_cwmin = _current_scheme->get_cwmin(p, tos);
    BRN_DEBUG("TosQM: opt_cwmin: %d\n", opt_cwmin);

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

          _all_bos[_all_bos_idx] = opt_cwmin;
          _all_bos_idx=(_all_bos_idx+1)%TOS2QM_BOBUF_SIZE;

          return p;
    }
  }

  BRN_DEBUG("optCW: %d",opt_cwmin);

  // handle trunc overflow
  if (need_recalc(opt_cwmin, tos)) {
    recalc_backoff_queues(opt_cwmin);
    set_backoff();
    if (BRN_DEBUG_LEVEL_DEBUG) print_queues();

  }

  opt_queue = find_queue(opt_cwmin); // queues changed, find opt queue again
  BRN_DEBUG("optQueue: %d",opt_queue);

  /**
   * Apply tos;
   */

  switch (tos) {
    case 1: opt_queue--;    break;
    case 2: opt_queue++;    break;
    case 3: opt_queue +=2 ; break;
  }
  opt_queue = MAX(0,MIN(opt_queue,no_queues-1));

  //set queue
  BrnWifi::setTxQueue(ceh, opt_queue);

  //add stats
  _queue_usage[opt_queue]++;
  _bo_usage_usage[_bo_exp[opt_queue]]++;
  //_last_bo_usage[_bo_exp[opt_queue]] = Timestamp::now();

  if (TOS2QM_ALL_BOS_STATS) {
    _all_bos[_all_bos_idx] = opt_cwmin;
    _all_bos_idx=(_all_bos_idx+1)%TOS2QM_BOBUF_SIZE;
  }

  _pkt_in_q++;
  return p;
}

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

  BRN_DEBUG("Tos2QM::handle_feedback()");

  if (_bqs_strategy == BACKOFF_STRATEGY_MEDIUMSHARE) {

    int no_transmissions     = 1; /* for wired */
    int no_rts_transmissions = 0;

    if (ceh->flags & WIFI_EXTRA_TX_ABORT)
      no_transmissions = (int)ceh->retries;
    else
      no_transmissions = (int)ceh->retries + 1;

    if (ceh->flags & WIFI_EXTRA_EXT_RETRY_INFO) {
      no_rts_transmissions = (int)(ceh->virt_col >> 4);
      //assert(no_transmissions == (int)(ceh->virt_col & 15));
      no_transmissions = (int)(ceh->virt_col & 15);
    }

    //BRN_DEBUG("  #trans: %d #RTS trans: %d\n", no_transmissions, no_rts_transmissions);

    if (no_rts_transmissions == 0)
      _current_scheme->handle_feedback(no_transmissions);

    if (no_rts_transmissions >= 0)
      _current_scheme->handle_feedback(no_rts_transmissions);
  }

  return;
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
  if ( _queue_mapping == QUEUEMAPPING_NEXT_SMALLER ) return find_queue_next_smaller(backoff_window_size);
  if ( _queue_mapping == QUEUEMAPPING_PROBABILISTIC ) return find_queue_prob(backoff_window_size, false);
  if ( _queue_mapping == QUEUEMAPPING_GRAVITATION ) return find_queue_prob(backoff_window_size, true);
  if ( _queue_mapping == QUEUEMAPPING_DIRECT ) return find_queue_next_smaller(backoff_window_size);

  return find_queue_next_bigger(backoff_window_size);
}

int
Tos2QueueMapper::find_queue_next_bigger(uint16_t backoff_window_size)
{
  if ( backoff_window_size <= _cwmin[0] ) return 0;

  // Take the first queue, whose cw-interval is in the range of the backoff-value
  for (int i = 0; i < no_queues-1; i++)
    if ( (_cwmin[i] < backoff_window_size) && (backoff_window_size <= _cwmin[i+1]) ) return i+1;

  return no_queues-1;
}

int
Tos2QueueMapper::find_queue_next_smaller(uint16_t backoff_window_size)
{
  if ( backoff_window_size <= _cwmin[0] ) return 0;

  // Take the first queue, whose cw-interval is in the range of the backoff-value
  for (int i = 0; i < no_queues-1; i++)
    if ((_cwmin[i] <= backoff_window_size) && (backoff_window_size < _cwmin[i+1])) return i;

  return no_queues-1;
}

int
Tos2QueueMapper::find_queue_prob(uint16_t backoff_window_size, bool quadratic_distance)
{
  if ( backoff_window_size <= _cwmin[0] ) return 0;

  // Take the first queue, whose cw-interval is in the range of the backoff-value
  for (int i = 0; i <= no_queues-1; i++) {
    if ( backoff_window_size > _cwmin[i] && backoff_window_size <= _cwmin[i+1] ) {
      int dist_lower_queue = ((uint32_t)backoff_window_size - (uint32_t)_cwmin[i]);
      int dist_upper_queue = ((uint32_t)_cwmin[i+1] - (uint32_t)backoff_window_size);

      if ( quadratic_distance ) {
        dist_lower_queue *= dist_lower_queue;
        dist_upper_queue *= dist_upper_queue;
      }

      if ( (click_random() % (dist_lower_queue + dist_upper_queue)) < (uint32_t)dist_lower_queue ) return i+1;

      return i;
    }
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

bool
Tos2QueueMapper::need_recalc(uint32_t bo, uint32_t /*tos*/)
{
  if ( _queue_mapping == QUEUEMAPPING_DIRECT ) return _cwmin[1] != bo;

  return (((uint32_t)_cwmin[0] >= bo) || ((uint32_t)_cwmin[no_queues-1] <= bo));
}

uint32_t
Tos2QueueMapper::recalc_backoff_queues(uint32_t backoff)
{
  int cwmin_tos_0;
  int cwmin_tos_1 = (backoff>0)?backoff:1;
  uint32_t fib_0 = 0;

  if ( _queue_mapping != QUEUEMAPPING_DIRECT) {
    cwmin_tos_1 = find_closest_backoff(cwmin_tos_1);
  }

  /* get tos 0 */
  switch (_qm_diff_queue_mode) {
    case QUEUEMAPPING_DIFFQUEUE_EXP:
      cwmin_tos_0 = (cwmin_tos_1 - 1) >> _qm_diff_queue_val;
      if (cwmin_tos_0 < 1) cwmin_tos_0 = 1;
      break;
    case QUEUEMAPPING_DIFFQUEUE_MUL:
      if ( _qm_diff_queue_val > 0 ) cwmin_tos_0 = cwmin_tos_1/_qm_diff_queue_val;
      else cwmin_tos_0 = cwmin_tos_1;
      break;
    case QUEUEMAPPING_DIFFQUEUE_ADD:
      cwmin_tos_0 = cwmin_tos_1 - _qm_diff_queue_val;
      break;
    case QUEUEMAPPING_DIFFQUEUE_FIB:
      for ( fib_0 = 0; fib_0 < FIBONACCI_VECTOR_SIZE; fib_0++ )
        if ( fibonacci_numbers[fib_0] >= backoff ) break;

      if ( fib_0 == FIBONACCI_VECTOR_SIZE ) fib_0 = MAX_FIBONACCI_INDEX;
      else fib_0 = (fib_0>=_qm_diff_queue_val)?(fib_0-_qm_diff_queue_val):0;

      cwmin_tos_0 = fibonacci_numbers[fib_0];

      break;
    default:
      BRN_ERROR("Unknown diff_queue_mode");
  }

  if (cwmin_tos_0 < 1) cwmin_tos_0 = 1;

  int cwmin = cwmin_tos_0;

  for ( int q = 0; q < no_queues; q++) {
    _cwmin[q] = cwmin;

    switch (_qm_diff_minmaxcw_mode) {
      case QUEUEMAPPING_DIFF_MINMAXCW_EXP:
        _cwmax[q] = ((cwmin+1) << _qm_diff_minmaxcw_val) - 1;
        break;
      case QUEUEMAPPING_DIFF_MINMAXCW_MUL:
        _cwmax[q] = (cwmin * _qm_diff_minmaxcw_val);
        break;
      case QUEUEMAPPING_DIFF_MINMAXCW_ADD:
        _cwmax[q] = (cwmin + _qm_diff_minmaxcw_val);
        break;
      case QUEUEMAPPING_DIFF_MINMAXCW_FIB:
        _cwmax[q] = fibonacci_numbers[MIN(fib_0 + _qm_diff_minmaxcw_val,MAX_FIBONACCI_INDEX)];
        break;
      default:
        BRN_ERROR("Unknown diff_minmax_mode");
    }

    switch (_qm_diff_queue_mode) {
      case QUEUEMAPPING_DIFFQUEUE_EXP:
        cwmin = ((cwmin+1) << _qm_diff_queue_val) - 1;
        break;
      case QUEUEMAPPING_DIFFQUEUE_MUL:
        cwmin = (cwmin * _qm_diff_queue_val);
        break;
      case QUEUEMAPPING_DIFFQUEUE_ADD:
        cwmin = (cwmin + _qm_diff_queue_val);
        break;
      case QUEUEMAPPING_DIFFQUEUE_FIB:
        fib_0 = MIN(fib_0 + _qm_diff_queue_val,MAX_FIBONACCI_INDEX);
        cwmin = fibonacci_numbers[fib_0];
        break;
      default:
        BRN_ERROR("Unknown diff_queue_mode");
    }

    _bo_exp[q] = MIN(_bo_usage_max_no-1, find_closest_backoff_exp(_cwmin[q]));
  }

  return 0;
}

uint32_t
Tos2QueueMapper::set_backoff()
{
  _call_set_backoff++;

  _device->set_backoff();

  return 0;
}

uint32_t
Tos2QueueMapper::get_backoff()
{
  /* Init queue stuff */
  _device->get_backoff();

  no_queues = _device->get_no_queues();

  if ( no_queues > 0 ) {
    _cwmin = _device->get_cwmin();
    _cwmax = _device->get_cwmax();
  }

  _aifs  = NULL;

  BRN_DEBUG("");
  for (uint8_t i = 0; i < no_queues; i++)
    BRN_DEBUG("Tos2QM.configure(): Q %d: %d %d\n", i, _cwmin[i], _cwmax[i]);

  return 0;
}

uint32_t
Tos2QueueMapper::set_mac_backoff_scheme(uint32_t scheme)
{
  _device->set_backoff_scheme(scheme);
  return 0;
}

/****************************************************************************************************************/
/*********************************** H A N D L E R **************************************************************/
/****************************************************************************************************************/

enum {H_TOS2QUEUEMAPPER_STATS, H_TOS2QUEUEMAPPER_RESET, H_TOS2QUEUEMAPPER_STRATEGY,
      H_TOS2QUEUEMAPPER_BOS, H_TOS2QUEUEMAPPER_TEST, H_TOS2QUEUEMAPPER_CONFIG};

String
Tos2QueueMapper::stats()
{
  StringAccum sa;
  sa << "<tos2queuemapper node=\"" << BRN_NODE_NAME << "\" strategy=\"" << _bqs_strategy << "\" queues=\"";
  sa << (uint32_t)no_queues << "\" queue_mapping=\"" << _queue_mapping << "\" feedback_cnt=\"";
  sa << _feedback_cnt << "\" tx_cnt=\"" << _tx_cnt << "\" packets_in_queue=\"" << _pkt_in_q;
  sa << "\" calls_set_backoff=\"" << _call_set_backoff << "\" >\n\t<queueusage>\n";
  for ( int i = 0; i < no_queues; i++) {
    sa << "\t\t<queue index=\"" << i << "\" usage=\"" << _queue_usage[i];
    sa << "\" cwmin=\"" << _cwmin[i] << "\" cwmax=\"" << _cwmax[i];
    sa << "\" bo_exp=\"" << _bo_exp[i] << "\" />\n";
  }
  sa << "\t</queueusage>\n";
  sa << "\t<backoffusage>\n";
  for ( uint32_t i = 0; i < _bo_usage_max_no; i++) {
    sa << "\t\t<backoff value=\"" << (uint32_t)((uint32_t)1 << i)-1  << "\" usage=\"" << _bo_usage_usage[i];
    sa << "\" exp=\"" << i;
    sa << "\" last_usage=\"" << _last_bo_usage[i].sec() << "\" />\n";
  }
  sa << "\t</backoffusage>\n";

  sa << "\t<strategies>\n";
    sa << "\t\t<strategy name=\"BoOff\" id=\"0\" active=\"" << (int)(_bqs_strategy==0?1:0) << "\" />\n";
    sa << "\t\t<strategy name=\"BoDirect\" id=\"1\" active=\"" << (int)(_bqs_strategy==1?1:0) << "\" />\n";
  for ( uint32_t i = 2; i <= _scheme_list._max_scheme_id; i++) {
    Element *e = (Element *)_scheme_list.get_scheme(i);
    if ( e == NULL ) continue;
    sa << "\t\t<strategy name=\"" << e->class_name() << "\" id=\"" << i;
    sa << "\" active=\"" << (int)(i==_bqs_strategy?1:0) << "\" />\n";
  }
  sa << "\t</strategies>\n</tos2queuemapper>\n";
  return sa.take_string();
}

String
Tos2QueueMapper::bos()
{
  StringAccum sa;

  if (!TOS2QM_ALL_BOS_STATS) {
    sa << "error:  stats flag wasn't set\n";
    return sa.take_string();
  }

  sa << "<tos2queuemapper_bos node=\"" << BRN_NODE_NAME << "\" >\n";
  sa << "\t<all_bos>\n";
  sa << "\t\t<bos values=\"";
  for (uint32_t i = 0; i < TOS2QM_BOBUF_SIZE; i++) {
    if ( _all_bos[i] < 0 ) break;
    if (i > 0) sa << ",";
    sa << _all_bos[i];
  }
  sa << "\" />\n";
  sa << "\t</all_bos>\n";
  sa << "</tos2queuemapper_bos>\n";

  return sa.take_string();
}

void
Tos2QueueMapper::set_params(uint32_t q_map, uint32_t q_mode, uint32_t q_val, uint32_t cw_mode, uint32_t cw_val)
{
  _queue_mapping = q_map;
  _qm_diff_queue_mode = q_mode;
  _qm_diff_minmaxcw_mode = cw_mode;
  _qm_diff_queue_val = q_val;
  _qm_diff_minmaxcw_val = cw_val;
}

void
Tos2QueueMapper::print_queues()
{
  for ( int q = 0; q < no_queues; q++) {
    click_chatter("Queue: %d cwmin: %d cwmax: %d", q, _cwmin[q], _cwmax[q]);
  }
}

void
Tos2QueueMapper::test_params(uint32_t q_map, uint32_t q_mode, uint32_t q_val, uint32_t cw_mode, uint32_t cw_val, uint32_t tos)
{
  set_params(q_map, q_mode, q_val, cw_mode, cw_val);

  click_chatter("\nTest: QMAP: %d QMODE: %d QVAL: %d CW_MODE: %d CW_VAL: %d TOS: %d",
                                             q_map, q_mode, q_val, cw_mode, cw_val, tos);
  recalc_backoff_queues(tos);
  print_queues();

  click_chatter("\n");

}

void
Tos2QueueMapper::test()
{
  test_params(QUEUEMAPPING_NEXT_BIGGER,QUEUEMAPPING_DIFFQUEUE_EXP,1,QUEUEMAPPING_DIFF_MINMAXCW_EXP,1,16);
  test_params(QUEUEMAPPING_NEXT_BIGGER,QUEUEMAPPING_DIFFQUEUE_EXP,1,QUEUEMAPPING_DIFF_MINMAXCW_EXP,1,15);

  test_params(QUEUEMAPPING_NEXT_BIGGER,QUEUEMAPPING_DIFFQUEUE_EXP,2,QUEUEMAPPING_DIFF_MINMAXCW_EXP,2,16);

  test_params(QUEUEMAPPING_NEXT_BIGGER,QUEUEMAPPING_DIFFQUEUE_FIB,1,QUEUEMAPPING_DIFF_MINMAXCW_FIB,1,16);
  test_params(QUEUEMAPPING_NEXT_BIGGER,QUEUEMAPPING_DIFFQUEUE_FIB,3,QUEUEMAPPING_DIFF_MINMAXCW_FIB,3,16);

  test_params(QUEUEMAPPING_DIRECT,QUEUEMAPPING_DIFFQUEUE_ADD,0,QUEUEMAPPING_DIFF_MINMAXCW_ADD,0,12);

  test_params(QUEUEMAPPING_DIRECT,QUEUEMAPPING_DIFFQUEUE_ADD,2,QUEUEMAPPING_DIFF_MINMAXCW_ADD,3,12);

  test_params(QUEUEMAPPING_DIRECT,QUEUEMAPPING_DIFFQUEUE_MUL,2,QUEUEMAPPING_DIFF_MINMAXCW_MUL,4,10);
}

static String Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  switch ((uintptr_t) thunk) {
    case H_TOS2QUEUEMAPPER_STATS:
      return td->stats();
      break;
    case H_TOS2QUEUEMAPPER_BOS:
      return td->bos();
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
    case H_TOS2QUEUEMAPPER_TEST:
      f->test();
      break;
  }
  return 0;
}

void Tos2QueueMapper::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_read_handler("stats", Tos2QueueMapper_read_param, (void *) H_TOS2QUEUEMAPPER_STATS);//STATS:=statistics
  add_read_handler("bos", Tos2QueueMapper_read_param, (void *) H_TOS2QUEUEMAPPER_BOS);

  add_write_handler("queueconf", Tos2QueueMapper_write_param, (void *) H_TOS2QUEUEMAPPER_CONFIG);
  add_write_handler("reset", Tos2QueueMapper_write_param, (void *) H_TOS2QUEUEMAPPER_RESET, Handler::h_button);
  add_write_handler("strategy",Tos2QueueMapper_write_param, (void *)H_TOS2QUEUEMAPPER_STRATEGY);

  add_write_handler("test",Tos2QueueMapper_write_param, (void *)H_TOS2QUEUEMAPPER_TEST);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapper)
ELEMENT_MT_SAFE(Tos2QueueMapper)
