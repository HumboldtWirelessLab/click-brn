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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "tos2queuemapper.hh"

CLICK_DECLS

//TODO: wie kommen die Werte zustande?
static uint32_t tos2frac[] = { 63, 70, 77, 85 };

Tos2QueueMapper::Tos2QueueMapper():
    _cst(NULL),
    _colinf(NULL),
    pli(NULL),
    _bqs_strategy(0)
{
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
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  Vector<String> args;
  cp_spacevec(s_cwmin, args);

  no_queues = args.size();

  if ( no_queues > 0 ) {
    _cwmin = new uint16_t[no_queues];
    _cwmax = new uint16_t[no_queues];
    _aifs = new uint16_t[no_queues];

#warning TODO: Better check for params. Better Error handling.
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmin[i] = v;
    }

    cp_spacevec(s_cwmax, args);
    if ( args.size() < no_queues ) no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmax[i] = v;
    }

    cp_spacevec(s_aifs, args);
    if ( args.size() < no_queues ) no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _aifs[i] = v;
    }
  }

  _queue_usage = new uint32_t[no_queues];
  reset_queue_usage();

  return 0;
}

void Tos2QueueMapper::no_queues_set(uint8_t number)
{
	no_queues = number;
}


uint8_t Tos2QueueMapper::no_queues_get()
{
	return	no_queues;
}

void Tos2QueueMapper::queue_usage_set(uint8_t position, uint32_t value)
{
	if (position < no_queues) _queue_usage[position] = value;
}


uint32_t Tos2QueueMapper::queue_usage_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_queue_usage[position];
}

void Tos2QueueMapper::cwmin_set(uint8_t position, uint32_t value)
{
	if (position < no_queues) _cwmin[position] = value;
}


uint32_t Tos2QueueMapper::cwmin_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues -1;
	return	_cwmin[position];
}

void Tos2QueueMapper::cwmax_set(uint8_t position, uint32_t value)
{
	if (position <= no_queues) _cwmax[position] = value;
}


uint32_t Tos2QueueMapper::cwmax_get(uint8_t position)
{
	if (position >= no_queues) position = no_queues - 1;
	return	_cwmax[position];
}

Packet *
Tos2QueueMapper::simple_action(Packet *p)
{
  uint8_t tos = BRNPacketAnno::tos_anno(p);
  int opt_queue = tos;
  if (_bqs_strategy == 0) {
	BRN_DEBUG("optimale queue:= %d",opt_queue);
	for (int i = 0; i <= no_queues_get();i++) {
	BRN_DEBUG("cwmin[%d] := %d",i,cwmin_get(i));
	BRN_DEBUG("cwmax[%d] := %d",i,cwmax_get(i));
	BRN_DEBUG("queue_usage[%d] := %d",i,queue_usage_get(i));
	if( NULL != pli) BRN_DEBUG("PLI is not Null\n\r");
	if ( _cst != NULL ) {
    		struct airtime_stats *as;
    		as = _cst->get_latest_stats(); // _cst->get_stats(&as,0);//get airtime statisics
		BRN_DEBUG("Number of Neighbours %d",as->no_sources);  
	}

  }

}
else if (_bqs_strategy == 1) {
  if ( _colinf != NULL ) {
    if ( _colinf->_global_rs != NULL ) {
      uint32_t target_frac = tos2frac[tos];
      //find queue with min. frac of target_frac
      int ofq = -1;
      for ( int i = 0; i < no_queues; i++ ) {
        if (ofq == -1) {
          BRN_DEBUG("Foo");
          BRN_DEBUG("%p",_colinf->_global_rs);
          BRN_DEBUG("queu: %d",_colinf->_global_rs->get_frac(i));
          //int foo = _colinf->_global_rs->get_frac(i);
          if (_colinf->_global_rs->get_frac(i) >= target_frac) {
            if ( i == 0 ) {
              ofq = i;
            } else if ( _colinf->_global_rs->get_frac(i-1) == -1 ) {
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


enum {H_STATS, H_RESET};

static String
Tos2QueueMapper_read_param(Element *e, void *thunk)
{
  Tos2QueueMapper *td = (Tos2QueueMapper *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      StringAccum sa;

      sa << "<queueusage queues=\"" << (uint32_t)td->no_queues_get() << "\" >\n";

      for ( int i = 0; i < td->no_queues_get(); i++) {
        sa << "\t<queue index=\"" << i << "\" usage=\"" << td->queue_usage_get(i) << "\" />\n";
      }
      sa << "</queueusage>\n";
      return sa.take_string();
      break;
  }

  return String();
}

static int
Tos2QueueMapper_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Tos2QueueMapper *f = (Tos2QueueMapper *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_RESET: {
      f->reset_queue_usage();
      break;
    }
  }
  return 0;
}

void
Tos2QueueMapper::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("queue_usage", Tos2QueueMapper_read_param, (void *) H_STATS);
  add_write_handler("reset", Tos2QueueMapper_write_param, (void *) H_RESET);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapper)
ELEMENT_MT_SAFE(Tos2QueueMapper)
