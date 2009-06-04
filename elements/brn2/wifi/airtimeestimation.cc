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

#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/vector.hh>
#include <elements/wifi/bitrate.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include <click/error.hh>

#include "airtimeestimation.hh"

CLICK_DECLS

AirTimeEstimation::AirTimeEstimation()
{
}

AirTimeEstimation::~AirTimeEstimation()
{
}

int
AirTimeEstimation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _debug = false;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpBool, &_debug,
                     cpEnd);

  max_age = 5;
  oldest = Timestamp::now();
  return ret;
}

void
AirTimeEstimation::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  if ( ceh->flags & WIFI_EXTRA_TX ) {
    for ( int i = 0; i < (int) ceh->retries; i++ ) {
      int t0,t1,t2,t3;
      t0 = ceh->max_tries;
      t1 = t0 + ceh->max_tries1;
      t2 = t1 + ceh->max_tries2;
      t3 = t2 + ceh->max_tries3;

      PacketInfo *new_pi = new PacketInfo();
      new_pi->_rx_time.set_now(); //p->timestamp_anno()
      new_pi->_length = p->length();
      new_pi->_foreign = false;
      new_pi->_channel = BRNPacketAnno::channel_anno(p);

      if ( i < t0 ) new_pi->_rate = ceh->rate;
      else if ( i < t1 ) new_pi->_rate = ceh->rate1;
      else if ( i < t2 ) new_pi->_rate = ceh->rate2;
      else if ( i < t3 ) new_pi->_rate = ceh->rate3;

      new_pi->_duration = calc_transmit_time(new_pi->_rate, new_pi->_length);

      _packet_list.push_back(new_pi);
    }
  } else {
    PacketInfo *new_pi = new PacketInfo();

    new_pi->_rx_time.set_now(); //p->timestamp_anno()
    new_pi->_rate = ceh->rate;
    new_pi->_length = p->length();
    new_pi->_foreign = true;
    new_pi->_channel = BRNPacketAnno::channel_anno(p);

    new_pi->_duration = calc_transmit_time(new_pi->_rate, new_pi->_length);

    _packet_list.push_back(new_pi);
  }

  output(port).push(p);
}

void
AirTimeEstimation::clear_old()
{
  Timestamp now = Timestamp::now();
  Timestamp diff = now - oldest;

  if ( diff.sec() > max_age ) {
    int i;
    for ( i = 0; i < _packet_list.size(); i++) {
      PacketInfo *pi = _packet_list[i];
      diff = now - pi->_rx_time;
      if ( diff.sec() <= (max_age + 1) ) break; //TODO: exact calc (not +1)
    }

    if ( i > 0 ) {
      _packet_list.erase(_packet_list.begin() + (i-1));
    }

    if ( _packet_list.size() > 0 )
      oldest = _packet_list[0]->_rx_time;
    else
      oldest = Timestamp::now();
  }
}

String
AirTimeEstimation::stats()
{
  StringAccum sa;
  Timestamp now = Timestamp::now();
  Timestamp diff = now - oldest;

  uint32_t airtime;
  airtime = 0;
  int i;
  int size = 0;

  for ( i = 0; i < _packet_list.size(); i++) {
    PacketInfo *pi = _packet_list[i];
    diff = now - pi->_rx_time;
//    click_chatter("DIff: %d",diff.sec());
    if ( diff.sec() <= (max_age) )  break; //TODO: exact calc (not +1)
  }

  size = _packet_list.size() - i;
  for (; i < _packet_list.size(); i++) {
    PacketInfo *pi = _packet_list[i];
    airtime += pi->_duration; 
  }

  sa << "AllPackets: " << _packet_list.size() << "\n";
  sa << "Size: " << size << "\n";
  sa << "airtime in 5: " << airtime << "\n";
  sa << "airtime/s: " << (airtime/5) << "\n";


  return sa.take_string();
}

void 
AirTimeEstimation::reset()
{
  _packet_list.clear();
}

enum {H_DEBUG, H_STATS, H_RESET};

static String 
AirTimeEstimation_read_param(Element *e, void *thunk)
{
  AirTimeEstimation *td = (AirTimeEstimation *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_STATS:
      return td->stats();
    default:
      return String();
  }
}

static int 
AirTimeEstimation_write_param(const String &in_s, Element *e, void *vparam,
                                  ErrorHandler *errh)
{
  AirTimeEstimation *f = (AirTimeEstimation *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {    //debug
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
    case H_RESET: {    //reset
      f->reset();
    }
  }
  return 0;
}

void
AirTimeEstimation::add_handlers()
{
  add_read_handler("debug", AirTimeEstimation_read_param, (void *) H_DEBUG);
  add_read_handler("stats", AirTimeEstimation_read_param, (void *) H_STATS);

  add_write_handler("debug", AirTimeEstimation_write_param, (void *) H_DEBUG);
  add_write_handler("reset", AirTimeEstimation_write_param, (void *) H_RESET, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AirTimeEstimation)
ELEMENT_MT_SAFE(AirTimeEstimation)
