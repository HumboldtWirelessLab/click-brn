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

#include "channelstats.hh"

CLICK_DECLS

ChannelStats::ChannelStats():
    hw_busy(0),
    hw_rx(0),
    hw_tx(0),
    max_age(1000)
{
}

ChannelStats::~ChannelStats()
{
}

int
ChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _debug = false;

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
ChannelStats::push(int port, Packet *p)
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

     struct click_wifi *w = (struct click_wifi *) p->data();

      new_pi->_src = EtherAddress(w->i_addr2);

      if ( ceh->magic == WIFI_EXTRA_MAGIC && ( (ceh->flags & WIFI_EXTRA_RX_ERR) == 0 ))
        new_pi->_state = STATE_OK;
      else if ( ceh->magic == WIFI_EXTRA_MAGIC && (ceh->flags & WIFI_EXTRA_RX_ERR) &&
               (ceh->flags & WIFI_EXTRA_RX_CRC_ERR) )
        new_pi->_state = STATE_CRC;
      else if ( ceh->magic == WIFI_EXTRA_MAGIC && (ceh->flags & WIFI_EXTRA_RX_ERR) &&
               (ceh->flags & WIFI_EXTRA_RX_PHY_ERR) )
        new_pi->_state = STATE_PHY;

      new_pi->_noise = ceh->silence;
      new_pi->_rssi = (signed char)ceh->rssi;

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
ChannelStats::clear_old()
{
  Timestamp now = Timestamp::now();
  Timestamp diff;

  if ( _packet_list.size() == 0 ) return;

  PacketInfo *pi = _packet_list[0];
  diff = now - pi->_rx_time;

  int deletes = 0;
  if ( diff.msecval() > max_age ) {
    int i;
    for ( i = 0; i < _packet_list.size(); i++) {
      pi = _packet_list[i];
      diff = now - pi->_rx_time;
      if ( diff.msecval() <= max_age ) break; //TODO: exact calc
      else {
        delete pi;
        deletes++;
      }
    }

    if ( i > 0 ) _packet_list.erase(_packet_list.begin(), _packet_list.begin() + i);
  }
}


/*********************************************/
/************* HW BASED STATS ****************/
/*********************************************/
void
ChannelStats::addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx) {
  PacketInfoHW *new_pi = new PacketInfoHW();
  new_pi->_time = *time; //p->timestamp_anno()

  //click_chatter("STAT: %d %d %d",busy,rx,tx);

  hw_busy = busy;
  hw_rx = rx;
  hw_tx = tx;

  if ( _packet_list_hw.size() == 0 ) {
    new_pi->_busy = 1;
    new_pi->_rx = 1;
    new_pi->_tx = 1;
  } else {
    uint32_t diff = (new_pi->_time - _packet_list_hw[_packet_list_hw.size()-1]->_time).usecval();
    //click_chatter("DIFF: %d",diff);
    new_pi->_busy = (diff * busy) / 100;
    new_pi->_rx = (diff * rx) / 100;
    new_pi->_tx = (diff * tx) / 100;
  }

  _packet_list_hw.push_back(new_pi);
  clear_old_hw();
}

void
ChannelStats::clear_old_hw()
{
  Timestamp now = Timestamp::now();
  Timestamp diff;

  if ( _packet_list_hw.size() == 0 ) return;

  PacketInfoHW *pi = _packet_list_hw[0];
  diff = now - pi->_time;

  if ( diff.msecval() > max_age ) {
    int i;
    for ( i = 0; i < _packet_list_hw.size(); i++) {
      pi = _packet_list_hw[i];
      diff = now - pi->_time;
      if ( diff.msecval() <= max_age ) break; //TODO: exact calc
      else delete pi;
    }

    if ( i > 0 ) _packet_list_hw.erase(_packet_list_hw.begin(), _packet_list_hw.begin() + i);
  }
}

/*********************************************/
/************ CALCULATE STATS ****************/
/*********************************************/

enum {H_DEBUG, H_RESET, H_MAX_TIME, H_STATS, H_STATS_BUSY, H_STATS_RX, H_STATS_TX, H_STATS_HW_BUSY, H_STATS_HW_RX, H_STATS_HW_TX, H_STATS_AVG_NOISE, H_STATS_AVG_RSSI};

String
ChannelStats::stats_handler(int mode)
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
      sa << "Last HW Busy: " << hw_busy << "\n";
      sa << "Last HW RX: " << hw_rx << "\n";
      sa << "Last HW TX: " << hw_tx << "\n";
      sa << "Avg. Noise: " << stats.avg_noise << "\n";
      sa << "Avg. Rssi: " << stats.avg_rssi << "\n";
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
    case H_STATS_AVG_NOISE:
      sa << stats.avg_noise;
      break;
    case H_STATS_AVG_RSSI:
      sa << stats.avg_rssi;
      break;
  }
  return sa.take_string();
}

void
ChannelStats::calc_stats(struct airtime_stats *stats)
{
  Timestamp now = Timestamp::now();
  Timestamp diff;
  int i, rx_packets = 0;

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

    if ( pi->_rx) {
      stats->rx += pi->_duration;
      stats->avg_noise += pi->_noise;
      stats->avg_rssi += pi->_rssi;
      rx_packets++;
    } else stats->tx += pi->_duration;
  }

  int diff_time = 10 * max_age;
  stats->busy = stats->rx + stats->tx;
  stats->busy /= diff_time;
  stats->rx /= diff_time;
  stats->tx /= diff_time;

  if ( rx_packets > 0 ) {
    stats->avg_noise /= rx_packets;
    stats->avg_rssi /= rx_packets;
  } else {
    stats->avg_noise = -100; // default value
    stats->avg_rssi = 0;
  }

/******** HW ***********/

  if ( _packet_list_hw.size() == 0 ) {
    click_chatter("List null");
    return;
  }

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
ChannelStats::reset()
{
  _packet_list.clear();
}

static String 
ChannelStats_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  ChannelStats *td = (ChannelStats *)e;
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
    case H_STATS_AVG_NOISE:
    case H_STATS_AVG_RSSI:
      return td->stats_handler((uintptr_t) thunk);
    case H_MAX_TIME:
      return String(td->max_age) + "\n";
    default:
      return String();
  }
}

static int 
ChannelStats_write_param(const String &in_s, Element *e, void *vparam,
                                  ErrorHandler *errh)
{
  ChannelStats *f = (ChannelStats *)e;
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
ChannelStats::add_handlers()
{
  add_read_handler("debug", ChannelStats_read_param, (void *) H_DEBUG);
  add_read_handler("max_time", ChannelStats_read_param, (void *) H_MAX_TIME);
  add_read_handler("stats", ChannelStats_read_param, (void *) H_STATS);
  add_read_handler("busy", ChannelStats_read_param, (void *) H_STATS_BUSY);
  add_read_handler("rx", ChannelStats_read_param, (void *) H_STATS_RX);
  add_read_handler("tx", ChannelStats_read_param, (void *) H_STATS_TX);
  add_read_handler("hw_busy", ChannelStats_read_param, (void *) H_STATS_HW_BUSY);
  add_read_handler("hw_rx", ChannelStats_read_param, (void *) H_STATS_HW_RX);
  add_read_handler("hw_tx", ChannelStats_read_param, (void *) H_STATS_HW_TX);
  add_read_handler("avg_noise", ChannelStats_read_param, (void *) H_STATS_AVG_NOISE);
  add_read_handler("avg_rssi", ChannelStats_read_param, (void *) H_STATS_AVG_RSSI);

  add_write_handler("debug", ChannelStats_write_param, (void *) H_DEBUG);
  add_write_handler("reset", ChannelStats_write_param, (void *) H_RESET, Handler::BUTTON);
  add_write_handler("max_time", ChannelStats_write_param, (void *) H_MAX_TIME);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ChannelStats)
ELEMENT_MT_SAFE(ChannelStats)
