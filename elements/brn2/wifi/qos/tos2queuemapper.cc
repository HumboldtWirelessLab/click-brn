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
 * queuemapper.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "queuemapper.hh"

CLICK_DECLS

QueueMapper::QueueMapper()
{
}

QueueMapper::~QueueMapper()
{
}

int
QueueMapper::configure(Vector<String> &conf, ErrorHandler* errh)
{
  String s_cwmin;
  String s_cwmax;
  String s_aifs;
  int v;

  if (cp_va_kparse(conf, this, errh,
      "CWMIN", cpkM+cpkP, cpString, &s_cwmin,
      "CWMAX", cpkM+cpkP, cpString, &s_cwmax,
      "AIFS", cpkM+cpkP, cpString, &s_aifs,
      "CHANNELSTAT", cpkP, cpElement, &_cst,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  Vector<String> args;
  cp_spacevec(s_cwmin, args);

  no_queues = args.size();

  _cwmin = new uint16_t(no_queues);
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

  return 0;
}


Packet *
QueueMapper::simple_action(Packet *p)
{
  uint8_t tos = BRNPacketAnno::tos_anno(p);

  int opt_queue = tos;

  if ( _cst != null ) {
    struct airtime_stats as;
    _cst->get_stats(&as,0);

    int opt_cwmin = get_cwmin(as.frac_mac_busy, as.no_sources);
    opt_queue = find_queue(opt_cwmin);

    int diff_q = (no_queues / 2) - tos - 1;
    opt_queue -= diff_q;

    if ( opt_queue < 0 ) opt_queue = 0;
    else if ( opt_queue > no_queues ) opt_queue = no_queues;
  }
  
  if ( opt_queue > 4 ) opt_queue = 4;
  
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  BrnWifi::setTxQueue(ceh, 2);

  return p;
}

int
QueueMapper::get_cwmin(int /*busy*/, int /*nodes*/) {
  return 10;
}

int
QueueMapper::find_queue(int cwmin) {
  for ( int i = 0; i < no_queues; i++ ) {
    if ( _cwmin[i] > cwmin ) return i;
  }

  return no_queues - 1;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(QueueMapper)
ELEMENT_MT_SAFE(QueueMapper)
