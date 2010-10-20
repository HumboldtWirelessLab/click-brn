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

AirTimeEstimation::AirTimeEstimation():
    hw_busy(0),
    hw_rx(0),
    hw_tx(0)
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
  max_age = 5000;

  ret = cp_va_kparse(conf, this, errh,
                     "MAX_AGE", cpkP, cpInteger, &max_age,
                     "DEBUG", cpkP, cpBool, &_debug,
                     cpEnd);

  return ret;
}

/*********************************************/
/************* RX BASED STATS ****************/
/*********************************************/
void
AirTimeEstimation::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  if ( ceh->flags & WIFI_EXTRA_TX ) {

    for ( int i = 0; i < (int) ceh->retries; i++ ) {
      int t0,t1,t2,t3;
      t0 = ceh->max_tries + 1;
      t1 = t0 + ceh->max_tries1;
      t2 = t1 + ceh->max_tries2;
      t3 = t2 + ceh->max_tries3;

      PacketInfo *new_pi = new PacketInfo();
      new_pi->_rx_time = Timestamp::now(); //p->timestamp_anno()
      new_pi->_length = p->length();
      new_pi->_foreign = false;
      new_pi->_channel = BRNPacketAnno::channel_anno(p);
      new_pi->_rx = false;

      if ( i < t0 ) new_pi->_rate = ceh->rate;
      else if ( i < t1 ) new_pi->_rate = ceh->rate1;
      else if ( i < t2 ) new_pi->_rate = ceh->rate2;
      else if ( i < t3 ) new_pi->_rate = ceh->rate3;

      if ( new_pi->_rate != 0 ) {
        new_pi->_duration = calc_transmit_time(new_pi->_rate, new_pi->_length);
        _packet_list.push_back(new_pi);
      } else {
        click_chatter("ZeroRate: Retry: %d All Retries: %d",i, ceh->retries);
        delete new_pi;
      }
    }
  } else {
    PacketInfo *new_pi = new PacketInfo();

    new_pi->_rx_time = Timestamp::now(); //p->timestamp_anno()
    new_pi->_rate = ceh->rate;
    new_pi->_length = p->length() + 4;   //CRC
    new_pi->_foreign = true;
    new_pi->_channel = BRNPacketAnno::channel_anno(p);
    new_pi->_rx = true;

    if ( new_pi->_rate != 0 ) {
      new_pi->_duration = calc_transmit_time(new_pi->_rate, new_pi->_length);

    /*StringAccum sa;
      sa << "Rate: " << (int)ceh->rate << " Len: " << p->length() << " Duration: " << new_pi->_duration;
      click_chatter("P: %s",sa.take_string().c_str());
    */
      _packet_list.push_back(new_pi);
    } else {
      delete new_pi;
    }
  }

  clear_old();
  output(port).push(p);
}

void
AirTimeEstimation::clear_old()
{
  Timestamp now = Timestamp::now();
  Timestamp diff;

  if ( _packet_list.size() == 0 ) return;

  PacketInfo *pi = _packet_list[0];
  diff = now - pi->_rx_time;

  if ( diff.msecval() > max_age ) {
    int i;
    for ( i = 0; i < _packet_list.size(); i++) {
      PacketInfo *pi = _packet_list[i];
      diff = now - pi->_rx_time;
      if ( diff.msecval() <= max_age ) break; //TODO: exact calc
    }

    if ( i > 0 )  _packet_list.erase(_packet_list.begin(), _packet_list.begin() + (i-1));
  }
}


/*********************************************/
/************* HW BASED STATS ****************/
/*********************************************/
void
AirTimeEstimation::addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx) {
  PacketInfoHW *new_pi = new PacketInfoHW();
  new_pi->_time = *time; //p->timestamp_anno()

  hw_busy = busy;
  hw_rx = rx;
  hw_tx = tx;

  if ( _packet_list_hw.size() == 0 ) {
    new_pi->_busy = 1;
    new_pi->_rx = 1;
    new_pi->_tx = 1;
  } else {
    uint32_t diff = (new_pi->_time - _packet_list_hw[_packet_list_hw.size()-1]->_time).msecval();
    new_pi->_busy = (diff * busy) / 100;
    new_pi->_rx = (diff * rx) / 100;
    new_pi->_tx = (diff * tx) / 100;
  }

  _packet_list_hw.push_back(new_pi);
  clear_old_hw();
}

void
AirTimeEstimation::clear_old_hw()
{
  Timestamp now = Timestamp::now();
  Timestamp diff;

  if ( _packet_list_hw.size() == 0 ) return;

  PacketInfoHW *pi = _packet_list_hw[0];
  diff = now - pi->_time;

  if ( diff.msecval() > max_age ) {
    int i;
    for ( i = 0; i < _packet_list_hw.size(); i++) {
      PacketInfoHW *pi = _packet_list_hw[i];
      diff = now - pi->_time;
      if ( diff.msecval() <= max_age ) break; //TODO: exact calc
    }

    if ( i > 0 ) _packet_list_hw.erase(_packet_list_hw.begin(), _packet_list_hw.begin() + (i-1));
  }
}

/*********************************************/
/************ CALCULATE STATS ****************/
/*********************************************/

enum {H_DEBUG, H_RESET, H_MAX_TIME, H_STATS, H_STATS_BUSY, H_STATS_RX, H_STATS_TX, H_STATS_HW_BUSY, H_STATS_HW_RX, H_STATS_HW_TX};

String
AirTimeEstimation::stats_handler(int mode)
{
  StringAccum sa;
  struct airtime_stats stats;

  calc_stats(&stats);

  switch (mode) {
    case H_STATS:
      sa << "All packets: " << _packet_list.size() << "\n";
      sa << "Packets: " << stats.packets << "\n";
      sa << "Busy: " << stats.busy << "\n";
      sa << "RX: " << stats.rx << "\n";
      sa << "TX: " << stats.tx << "\n";
      sa << "HW Busy: " << stats.hw_busy << "\n";
      sa << "HW RX: " << stats.hw_rx << "\n";
      sa << "HW TX: " << stats.hw_tx << "\n";
      break;
    case H_STATS_BUSY:
      sa << stats.busy;
      break;
    case H_STATS_RX:
      sa << stats.rx;
      break;
    case H_STATS_TX:
      sa << stats.tx;
      break;
    case H_STATS_HW_BUSY:
      sa << stats.hw_busy;
      break;
    case H_STATS_HW_RX:
      sa << stats.hw_rx;
      break;
    case H_STATS_HW_TX:
      sa << stats.hw_tx;
      break;
  }
  return sa.take_string();
}

void
AirTimeEstimation::calc_stats(struct airtime_stats *stats)
{
  Timestamp now = Timestamp::now();
  Timestamp diff;
  int i;

  memset(stats, 0, sizeof(struct airtime_stats));

  if ( _packet_list.size() == 0 ) return;

  PacketInfo *pi = _packet_list[0];
  diff = now - pi->_rx_time;

  for ( i = 0; i < _packet_list.size(); i++) {
    pi = _packet_list[i];
    diff = now - pi->_rx_time;
    if ( diff.msecval() <= max_age )  break; //TODO: exact calc
  }

  stats->packets = _packet_list.size() - i;

  for (; i < _packet_list.size(); i++) {
    pi = _packet_list[i];
    if ( pi->_rx) stats->rx += pi->_duration;
    else stats->tx += pi->_duration;
  }

  int diff_time = 10 * max_age;
  stats->busy = stats->rx + stats->tx;
  stats->busy /= diff_time;
  stats->rx /= diff_time;
  stats->tx /= diff_time;

/******** HW ***********/

  if ( _packet_list_hw.size() == 0 ) return;

  PacketInfoHW *pi_hw = _packet_list_hw[0];
  diff = now - pi_hw->_time;

  for ( i = 0; i < _packet_list_hw.size(); i++) {
    pi_hw = _packet_list_hw[i];
    diff = now - pi_hw->_time;
    if ( diff.msecval() <= max_age )  break; //TODO: exact calc
  }

  for (; i < _packet_list_hw.size(); i++) {
    pi_hw = _packet_list_hw[i];
    stats->hw_busy += pi_hw->_busy;
    stats->hw_rx += pi_hw->_rx;
    stats->hw_tx += pi_hw->_tx;
  }

  stats->hw_busy /= diff_time;
  stats->hw_rx /= diff_time;
  stats->hw_tx /= diff_time;
}

void
AirTimeEstimation::reset()
{
  _packet_list.clear();
}

static String 
AirTimeEstimation_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  AirTimeEstimation *td = (AirTimeEstimation *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG:
      return String(td->_debug) + "\n";
    case H_STATS:
    case H_STATS_BUSY:
    case H_STATS_RX:
    case H_STATS_TX:
    case H_STATS_HW_BUSY:
    case H_STATS_HW_RX:
    case H_STATS_HW_TX:
      return td->stats_handler((uintptr_t) thunk);
    case H_MAX_TIME:
      return String(td->max_age) + "\n";
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
    case H_MAX_TIME: {
      int mt;
      if (!cp_integer(s, &mt))
        return errh->error("max time parameter must be integer");
      f->max_age = mt;
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
  add_read_handler("max_time", AirTimeEstimation_read_param, (void *) H_MAX_TIME);
  add_read_handler("stats", AirTimeEstimation_read_param, (void *) H_STATS);
  add_read_handler("busy", AirTimeEstimation_read_param, (void *) H_STATS_BUSY);
  add_read_handler("rx", AirTimeEstimation_read_param, (void *) H_STATS_RX);
  add_read_handler("tx", AirTimeEstimation_read_param, (void *) H_STATS_TX);
  add_read_handler("hw_busy", AirTimeEstimation_read_param, (void *) H_STATS_HW_BUSY);
  add_read_handler("hw_rx", AirTimeEstimation_read_param, (void *) H_STATS_HW_RX);
  add_read_handler("hw_tx", AirTimeEstimation_read_param, (void *) H_STATS_HW_TX);

  add_write_handler("debug", AirTimeEstimation_write_param, (void *) H_DEBUG);
  add_write_handler("reset", AirTimeEstimation_write_param, (void *) H_RESET, Handler::BUTTON);
  add_write_handler("max_time", AirTimeEstimation_write_param, (void *) H_MAX_TIME);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AirTimeEstimation)
ELEMENT_MT_SAFE(AirTimeEstimation)
