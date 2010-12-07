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
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>
#include <click/error.hh>
#include <click/userutils.hh>
#include <click/timer.hh>
#include <clicknet/wifi.h>

#include <elements/wifi/bitrate.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>

#include "channelstats.hh"

CLICK_DECLS

ChannelStats::ChannelStats():
    _save_duration(CS_DEFAULT_SAVE_DURATION),
    _stats_duration(CS_DEFAULT_STATS_DURATION),
    hw_busy(0),
    hw_rx(0),
    hw_tx(0),
    _rssi_per_neighbour(CS_DEFAULT_RSSI_PER_NEIGHBOUR),
    _proc_file(),
    _proc_read(CS_DEFAULT_PROCREAD),
    _min_update_time(CS_DEFAULT_MIN_UPDATE_TIME),
    _stats_timer_enable(CS_DEFAULT_STATS_TIMER),
    _stats_timer(this),
    _stats_id(0),
    _channel(0),
    _fast_mode(false)
{
  stats.last_update = _last_hw_stat_time = _last_packet_time = Timestamp::now();
}

ChannelStats::~ChannelStats()
{
}

int
ChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  int proc_interval = CS_DEFAULT_PROCINTERVAL;
  _debug = false;

  ret = cp_va_kparse(conf, this, errh,
                     "STATS_DURATION", cpkP, cpInteger, &_stats_duration,
                     "SAVE_DURATION", cpkP, cpInteger, &_save_duration,
                     "PROCFILE", cpkP, cpString, &_proc_file,
                     "PROCINTERVAL", cpkP, cpInteger, &proc_interval,
                     "RSSI_PER_NEIGHBOUR", cpkP, cpBool, &_rssi_per_neighbour,
                     "STATS_TIMER", cpkP, cpBool, &_stats_timer_enable,
                     "MIN_UPDATE_TIME", cpkP, cpInteger, &_min_update_time,
                     "FAST_MODE", cpkP, cpBool, &_fast_mode,
                     "DEBUG", cpkP, cpBool, &_debug,
                     cpEnd);

  _proc_read = proc_interval != CS_DEFAULT_PROCINTERVAL;

  if ( _stats_timer_enable && _proc_read )  /*we use stats timer for proc so if stats timer then proc time = stats_time*/
    proc_interval = _stats_duration;

  if ( _proc_read )
    _stats_interval = proc_interval;
  else if ( _stats_timer_enable ) _stats_interval = _stats_duration;

  if ( _save_duration < _stats_duration ) _save_duration = _stats_duration;

  // init fast mode
  _sw_sum_rx_duration = 0;
  _sw_sum_tx_duration = 0;

  _sw_sum_rx_packets = 0;
  _sw_sum_tx_packets = 0;

  _sw_sum_rx_noise = 0;
  _sw_sum_rx_rssi = 0;

  return ret;
}

int
ChannelStats::initialize(ErrorHandler *)
{
  _stats_timer.initialize(this);

  if ( _stats_interval > 0 )
    _stats_timer.schedule_after_msec(_stats_interval);
  return 0;
}

void
ChannelStats::run_timer(Timer */*t*/)
{
  if ( _proc_read) readProcHandler();
  if ( _stats_timer_enable ) {
    _stats_id++;
    if (_rssi_per_neighbour)
      calc_stats(&stats, &rssi_tab);
    else
      calc_stats(&stats, NULL);
  }

  if ( _stats_interval > 0 )
    _stats_timer.schedule_after_msec(_stats_interval);
}

/*********************************************/
/************* RX BASED STATS ****************/
/*********************************************/
void
ChannelStats::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  _last_packet_time = p->timestamp_anno();

  if ( ceh->flags & WIFI_EXTRA_TX ) { // tx feedback packets

    for ( int i = 0; i < (int) ceh->retries; i++ ) {
      int t0,t1,t2,t3;
      t0 = ceh->max_tries + 1;
      t1 = t0 + ceh->max_tries1;
      t2 = t1 + ceh->max_tries2;
      t3 = t2 + ceh->max_tries3;

      if (!_fast_mode) { // default mode: save everything in vector
	      PacketInfo *new_pi = new PacketInfo();
	      new_pi->_rx_time = p->timestamp_anno();
	      new_pi->_length = p->length();
	      new_pi->_foreign = false;
	      new_pi->_channel = _channel = BRNPacketAnno::channel_anno(p);
	      new_pi->_rx = false;

	      if ( i < t0 ) new_pi->_rate = ceh->rate;
	      else if ( i < t1 ) new_pi->_rate = ceh->rate1;
	      else if ( i < t2 ) new_pi->_rate = ceh->rate2;
	      else if ( i < t3 ) new_pi->_rate = ceh->rate3;

	      if ( new_pi->_rate != 0 ) {
	        new_pi->_duration = calc_transmit_time(new_pi->_rate, new_pi->_length);
	        _packet_list.push_back(new_pi);
	      } else {
	        //click_chatter("ZeroRate: Retry: %d All Retries: %d",i, ceh->retries);
	        delete new_pi;
	      }
	} else { // fast mode: incremental update
		_channel = BRNPacketAnno::channel_anno(p);
	       uint16_t rate = 0;
	       if ( i < t0 ) rate = ceh->rate;
	       else if ( i < t1 ) rate = ceh->rate1;
	       else if ( i < t2 ) rate = ceh->rate2;
	       else if ( i < t3 ) rate = ceh->rate3;
		if (rate != 0) {
			// update duration counter
			_sw_sum_tx_duration += calc_transmit_time(rate, p->length());
			_sw_sum_tx_packets++;
		}
	}
    }
  } else { // RX

	if (!_fast_mode) {
	    PacketInfo *new_pi = new PacketInfo();

	    new_pi->_rx_time = p->timestamp_anno();
	    new_pi->_rate = ceh->rate;
	    new_pi->_length = p->length() + 4;   //CRC
	    new_pi->_foreign = true;
	    new_pi->_channel = _channel = BRNPacketAnno::channel_anno(p);
	    new_pi->_rx = true;

	    if ( new_pi->_rate != 0 ) {
	      new_pi->_duration = calc_transmit_time(new_pi->_rate, new_pi->_length);

	      struct click_wifi *w = (struct click_wifi *) p->data();

	      new_pi->_src = EtherAddress(w->i_addr2);

	      if ( ceh->magic == WIFI_EXTRA_MAGIC && ( (ceh->flags & WIFI_EXTRA_RX_ERR) == 0 ))
       	 new_pi->_state = STATE_OK;
	      else if ( ceh->magic == WIFI_EXTRA_MAGIC && (ceh->flags & WIFI_EXTRA_RX_ERR) && (ceh->flags & WIFI_EXTRA_RX_CRC_ERR) )
	        new_pi->_state = STATE_CRC;
	      else if ( ceh->magic == WIFI_EXTRA_MAGIC && (ceh->flags & WIFI_EXTRA_RX_ERR) && (ceh->flags & WIFI_EXTRA_RX_PHY_ERR) )
	        new_pi->_state = STATE_PHY;

	      new_pi->_noise = (signed char)ceh->silence;
	      new_pi->_rssi = (signed char)ceh->rssi;

	      _packet_list.push_back(new_pi);
	    } else {
	      delete new_pi;
	    }
	} else { // fast mode
		if ( ceh->rate != 0 ) {
			_channel = BRNPacketAnno::channel_anno(p);
			// update duration counter
			_sw_sum_rx_duration += calc_transmit_time(ceh->rate, p->length() + 4);
			_sw_sum_rx_packets++;
			int foo = (signed char)ceh->silence;
			_sw_sum_rx_noise += foo;
//click_chatter("-> %d %d\n", foo, _sw_sum_rx_noise);
			_sw_sum_rx_rssi += (signed char)ceh->rssi;
		 }
	}
  }

  if (!_fast_mode) {
     clear_old();
  }
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
  if ( diff.msecval() > _save_duration ) {
    int i;
    for ( i = 0; i < _packet_list.size(); i++) {
      pi = _packet_list[i];
      diff = now - pi->_rx_time;
      if ( diff.msecval() <= _save_duration ) break; //TODO: exact calc
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
  new_pi->_time = *time;

  hw_busy = busy;
  hw_rx = rx;
  hw_tx = tx;
  _last_hw_stat_time = *time;

  if ( _packet_list_hw.size() == 0 ) {
    new_pi->_busy = 1;
    new_pi->_rx = 1;
    new_pi->_tx = 1;
  } else {
    uint32_t diff = (new_pi->_time - _packet_list_hw[_packet_list_hw.size()-1]->_time).usecval();
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

  if ( diff.msecval() > _save_duration ) {
    int i;
    for ( i = 0; i < _packet_list_hw.size(); i++) {
      pi = _packet_list_hw[i];
      diff = now - pi->_time;
      if ( diff.msecval() <= _save_duration ) break; //TODO: exact calc
      else delete pi;
    }

    if ( i > 0 ) _packet_list_hw.erase(_packet_list_hw.begin(), _packet_list_hw.begin() + i);
  }
}

/*************** PROC HANDLER ****************/

void
ChannelStats::readProcHandler()
{
  String raw_info = file_string(_proc_file);
  Vector<String> args;
  Timestamp now = Timestamp::now();

  int busy, rx, tx;

  cp_spacevec(raw_info, args);

  if ( args.size() > 6 ) {
    cp_integer(args[1],&busy);
    cp_integer(args[3],&rx);
    cp_integer(args[5],&tx);

    addHWStat(&now, busy, rx, tx);
  }
}

/*********************************************/
/************ CALCULATE STATS ****************/
/*********************************************/

void
ChannelStats::get_stats(struct airtime_stats *cstats, int /*time*/) {
  calc_stats(cstats, NULL);
}

void
ChannelStats::calc_stats(struct airtime_stats *cstats, RSSITable *rssi_tab)
{
  Timestamp now = Timestamp::now();
  Timestamp diff;
  int i, rx_packets = 0;

  HashMap<EtherAddress, EtherAddress> sources;

  diff = now - cstats->last_update;

  if ( diff.usecval() <= _min_update_time * 1000 ) return;

  if ( rssi_tab ) rssi_tab->clear();

  memset(cstats, 0, sizeof(struct airtime_stats));

  cstats->last_update = now;
  int diff_time = 10 * _stats_duration;

  /******** SW ***********/
  if (!_fast_mode) {
	  if ( _packet_list.size() == 0 ) return;

	  PacketInfo *pi = _packet_list[0];
	  diff = now - pi->_rx_time;

	  for ( i = 0; i < _packet_list.size(); i++) {
	    pi = _packet_list[i];
	    diff = now - pi->_rx_time;
	    if ( diff.msecval() <= _stats_duration )  break; //TODO: exact calc
	  }

	  cstats->packets = _packet_list.size() - i;

	  for (; i < _packet_list.size(); i++) {
	    pi = _packet_list[i];

	    if ( pi->_rx) {
	      cstats->rx += pi->_duration;
	      cstats->avg_noise += pi->_noise;
	      cstats->avg_rssi += pi->_rssi;
	      rx_packets++;

	      if ( sources.findp(pi->_src) == NULL ) sources.insert(pi->_src,pi->_src);

	      if ( rssi_tab != NULL ) {
	        SrcInfo *src_i;
	        if ( (src_i = rssi_tab->findp(pi->_src)) == NULL ) {
	          rssi_tab->insert(pi->_src, SrcInfo(pi->_rssi,1));
	        } else {
	          src_i->add_rssi(pi->_rssi);
	        }
	      }

	    } else cstats->tx += pi->_duration;
	  }

	  cstats->busy = cstats->rx + cstats->tx;
	  cstats->busy /= diff_time;
	  cstats->rx /= diff_time;
	  cstats->tx /= diff_time;

	  if ( cstats->busy > 100 ) cstats->busy = 100;
	  if ( cstats->rx > 100 ) cstats->rx = 100;
	  if ( cstats->tx > 100 ) cstats->tx = 100;

	  if ( rx_packets > 0 ) {
	    cstats->avg_noise /= rx_packets;
      if ( cstats->avg_noise > 120 ) cstats->avg_noise = 100;
      else if ( cstats->avg_noise < -120 ) cstats->avg_noise = -120;

      cstats->avg_rssi /= rx_packets;
      if ( cstats->avg_rssi > 120 ) cstats->avg_rssi = 100;
      else if ( cstats->avg_rssi < -120 ) cstats->avg_rssi = -120;

	  } else {
	    cstats->avg_noise = -100; // default value
	    cstats->avg_rssi = 0;
	  }

	  cstats->last = _packet_list[_packet_list.size()-1]->_rx_time;

	  cstats->no_sources = sources.size();
  } else { // fast mode

    cstats->busy = _sw_sum_rx_duration + _sw_sum_tx_duration;
	  cstats->busy /= 1e4;
	  cstats->rx = _sw_sum_rx_duration;
	  cstats->rx /= 1e4;
	  cstats->tx = _sw_sum_tx_duration;
	  cstats->tx /= 1e4;

	  if ( cstats->busy > 100 ) cstats->busy = 100;
	  if ( cstats->rx > 100 ) cstats->rx = 100;
	  if ( cstats->tx > 100 ) cstats->tx = 100;

	  if ( _sw_sum_rx_packets > 0 ) {
      cstats->avg_noise = _sw_sum_rx_noise;
      cstats->avg_noise /= _sw_sum_rx_packets;
      if ( cstats->avg_noise > 120 ) cstats->avg_noise = 120;
      else if ( cstats->avg_noise < -120 ) cstats->avg_noise = -120;

      cstats->avg_rssi = _sw_sum_rx_rssi;
      cstats->avg_rssi /= _sw_sum_rx_packets;
      if ( cstats->avg_rssi > 120 ) cstats->avg_rssi = 120;
      else if ( cstats->avg_rssi < -120 ) cstats->avg_rssi = -120;

	 } else {
	    cstats->avg_noise = -100; // default value
	    cstats->avg_rssi = 0;
	  }

	  cstats->last = _last_packet_time;
	  cstats->no_sources = -1; // Tbd. sources.size();

	  //click_chatter("Resetting at %d \n", curr_sec);
	  // start new bucket; reset all counters
    _sw_sum_rx_duration = 0;
    _sw_sum_tx_duration = 0;
	  _sw_sum_rx_packets = 0;
	  _sw_sum_tx_packets = 0;
	  _sw_sum_rx_noise = 0;
	  _sw_sum_rx_rssi = 0;
  }
  /******** HW ***********/

  cstats->hw_available = (_packet_list_hw.size() != 0);
  if ( _packet_list_hw.size() == 0 ) {
    //click_chatter("List null");
    return;
  }

  PacketInfoHW *pi_hw = _packet_list_hw[0];
  diff = now - pi_hw->_time;

  for ( i = 0; i < _packet_list_hw.size(); i++) {
    pi_hw = _packet_list_hw[i];
    diff = now - pi_hw->_time;
    if ( diff.msecval() <= _stats_duration )  break; //TODO: exact calc
  }

  for (; i < _packet_list_hw.size(); i++) {
    pi_hw = _packet_list_hw[i];
    cstats->hw_busy += pi_hw->_busy;
    cstats->hw_rx += pi_hw->_rx;
    cstats->hw_tx += pi_hw->_tx;
  }

  cstats->hw_busy /= diff_time;
  cstats->hw_rx /= diff_time;
  cstats->hw_tx /= diff_time;

  if ( cstats->hw_busy > 100 ) cstats->hw_busy = 100;
  if ( cstats->hw_rx > 100 ) cstats->hw_rx = 100;
  if ( cstats->hw_tx > 100 ) cstats->hw_tx = 100;

  cstats->last_hw = _packet_list_hw[_packet_list_hw.size()-1]->_time;
}

void
ChannelStats::reset()
{
  _packet_list.clear();
}

/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum {H_DEBUG, H_RESET, H_MAX_TIME, H_STATS, H_STATS_BUSY, H_STATS_RX, H_STATS_TX, H_STATS_HW_BUSY, H_STATS_HW_RX, H_STATS_HW_TX, H_STATS_AVG_NOISE, H_STATS_AVG_RSSI, H_STATS_XML, H_STATS_NO_SOURCES, H_STATS_NODES_RSSI, H_STATS_CHANNEL};

String
ChannelStats::stats_handler(int mode)
{
  StringAccum sa;

  if ( ! _stats_timer_enable ) {
    if (_rssi_per_neighbour)
      calc_stats(&stats, &rssi_tab);
    else
      calc_stats(&stats, NULL);
  }

  switch (mode) {
    case H_STATS:
     if ( _stats_timer_enable ) sa << "ID: " << _stats_id << "\n";
      sa << "Time: " << _stats_duration << "\n";
      sa << "All packets: " << _packet_list.size() << "\n";
      sa << "Packets: " << stats.packets << "\n";
      sa << "Busy: " << stats.busy << "\n";
      sa << "RX: " << stats.rx << "\n";
      sa << "TX: " << stats.tx << "\n";
      sa << "HW available: " << stats.hw_available << "\n";
      sa << "HW Busy: " << stats.hw_busy << "\n";
      sa << "HW RX: " << stats.hw_rx << "\n";
      sa << "HW TX: " << stats.hw_tx << "\n";
      sa << "Last HW Busy: " << hw_busy << "\n";
      sa << "Last HW RX: " << hw_rx << "\n";
      sa << "Last HW TX: " << hw_tx << "\n";
      sa << "Avg. Noise: " << stats.avg_noise << "\n";
      sa << "Avg. Rssi: " << stats.avg_rssi << "\n";
      sa << "No. Src: " << stats.no_sources << "\n";
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
    case H_STATS_NO_SOURCES:
      sa << stats.no_sources;
      break;
    case H_STATS_NODES_RSSI:
       sa << "etheraddress\tavg_rssi\tsum_rssi\tpkt_count\n";
      for (RSSITableIter iter = rssi_tab.begin(); iter.live(); iter++) {
        SrcInfo src = iter.value();
        EtherAddress ea = iter.key();

        sa << ea.unparse() << "\t" << src.avg_rssi() << "\t" << src._rssi << "\t" << src._pkt_count << "\n";
      }
      break;
    case H_STATS_XML:
      sa << "<stats id=\"" << _stats_id << "\" length=\"" << _stats_interval << "\" >\n";
      sa << "\t<channelload busy=\"" << stats.busy << "\" rx=\"" << stats.rx << "\" tx=\"" << stats.tx << "\" ";
      sa << "last_packet_time=\"" << _last_packet_time.unparse() << "\" ";
      sa << "hwbusy=\"" << stats.hw_busy << "\" hwrx=\"" << stats.hw_rx << "\" hwtx=\"" << stats.hw_tx << "\" ";
      sa << "last_hw_stat_time=\"" << _last_hw_stat_time.unparse() << "\" ";
      sa << "avg_noise=\"" << stats.avg_noise << "\" avg_rssi=\"" << stats.avg_rssi << "\" ";
      sa << "no_src=\"" << stats.no_sources << "\" channel=\"" << _channel << "\" />";
      sa << "\n\t<rssi>\n";
      for (RSSITableIter iter = rssi_tab.begin(); iter.live(); iter++) {
        SrcInfo src = iter.value();
        EtherAddress ea = iter.key();
        sa << "\t\t<nb addr=\"" << ea.unparse() << "\" rssi=\"" << src.avg_rssi() << "\" sum_rssi=\"" << src._rssi << "\" pkt_cnt=\"" << src._pkt_count << "\" />\n";
      }
      sa << "\t</rssi>\n</stats>\n";

  }
  return sa.take_string();
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
    case H_STATS_XML:
    case H_STATS_BUSY:
    case H_STATS_RX:
    case H_STATS_TX:
    case H_STATS_HW_BUSY:
    case H_STATS_HW_RX:
    case H_STATS_HW_TX:
    case H_STATS_AVG_NOISE:
    case H_STATS_AVG_RSSI:
    case H_STATS_NO_SOURCES:
    case H_STATS_NODES_RSSI:
      return td->stats_handler((uintptr_t) thunk);
    case H_STATS_CHANNEL:
      return String( td->get_channel());
      break;
    case H_MAX_TIME:
      return String(td->_stats_duration) + "\n";
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
      f->_stats_duration = mt;
      break;
    }
    case H_STATS_CHANNEL: {
      int channel;
      if (!cp_integer(s, &channel))
        return errh->error("channel1 parameter must be integer");
      f->set_channel(channel);
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
  add_read_handler("stats_xml", ChannelStats_read_param, (void *) H_STATS_XML);
  add_read_handler("busy", ChannelStats_read_param, (void *) H_STATS_BUSY);
  add_read_handler("rx", ChannelStats_read_param, (void *) H_STATS_RX);
  add_read_handler("tx", ChannelStats_read_param, (void *) H_STATS_TX);
  add_read_handler("hw_busy", ChannelStats_read_param, (void *) H_STATS_HW_BUSY);
  add_read_handler("hw_rx", ChannelStats_read_param, (void *) H_STATS_HW_RX);
  add_read_handler("hw_tx", ChannelStats_read_param, (void *) H_STATS_HW_TX);
  add_read_handler("avg_noise", ChannelStats_read_param, (void *) H_STATS_AVG_NOISE);
  add_read_handler("avg_rssi", ChannelStats_read_param, (void *) H_STATS_AVG_RSSI);
  add_read_handler("no_src", ChannelStats_read_param, (void *) H_STATS_NO_SOURCES);
  add_read_handler("src_rssi", ChannelStats_read_param, (void *) H_STATS_NODES_RSSI);
  add_read_handler("channel", ChannelStats_read_param, (void *) H_STATS_CHANNEL);

  add_write_handler("debug", ChannelStats_write_param, (void *) H_DEBUG);
  add_write_handler("reset", ChannelStats_write_param, (void *) H_RESET, Handler::BUTTON);
  add_write_handler("max_time", ChannelStats_write_param, (void *) H_MAX_TIME);
  add_write_handler("channel", ChannelStats_write_param, (void *) H_STATS_CHANNEL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ChannelStats)
ELEMENT_MT_SAFE(ChannelStats)
