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

#if CLICK_NS
#include <click/router.hh>
#endif

#include <elements/brn/brn2.h>
#include <elements/wifi/bitrate.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>

#include "channelstats.hh"

CLICK_DECLS

ChannelStats::ChannelStats():
    _device(NULL),
    _stats_interval(CS_DEFAULT_STATS_DURATION),
    _proc_file("none"),
    _proc_read(false),
    _proc_interval(CS_DEFAULT_STATS_DURATION),
    _neighbour_stats(CS_DEFAULT_RSSI_PER_NEIGHBOUR),
    _enable_full_stats(false),
    _save_duration(CS_DEFAULT_SAVE_DURATION),
    _min_update_time(CS_DEFAULT_MIN_UPDATE_TIME),
    _stats_id(0),
    _channel(0),
    _current_small_stats(0),
    _stats_timer(this),
    _proc_timer(static_proc_timer_hook,this)
{
  BRNElement::init();
  memset(_small_stats,0,sizeof(_small_stats));
  _full_stats.last_update = Timestamp::now();
}

ChannelStats::~ChannelStats()
{
}

int
ChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "DEVICE", cpkP, cpElement, &_device,
                     "STATS_DURATION", cpkP, cpInteger, &_stats_interval,  //default time, over which the stats are calculated
                     "PROCFILE", cpkP, cpString, &_proc_file,              //procfile you want to read from
                     "PROCINTERVAL", cpkP, cpInteger, &_proc_interval,     //how often the proc file is read and small stats
                     "NEIGHBOUR_STATS", cpkP, cpBool, &_neighbour_stats, //store/calc rssi per neighbour
                     "FULL_STATS", cpkP, cpBool, &_enable_full_stats,          //calculat full stats, store every thing for that
                     "SAVE_DURATION", cpkP, cpInteger, &_save_duration,    //max time, which a packetinfo is stored
                     "MIN_UPDATE_TIME", cpkP, cpInteger, &_min_update_time,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  _proc_read = (_proc_file != "none");
  if ( _save_duration < _stats_interval ) _save_duration = _stats_interval;
  if ( _proc_interval > _stats_interval ) _proc_interval = _stats_interval;

  return ret;
}

int
ChannelStats::initialize(ErrorHandler *)
{
  reset();

  _stats_timer.initialize(this);
  _proc_timer.initialize(this);

  if ( (!_enable_full_stats) || (_proc_read && (_stats_interval == _proc_interval)) )
    _stats_timer.schedule_after_msec(_stats_interval);

  if ( _proc_read && (_stats_interval != _proc_interval) ) _proc_timer.schedule_after_msec(_proc_interval);

  if ( ! _enable_full_stats ) {
    //INIT: clear current stats
    memset(&(_small_stats[_current_small_stats]),0,sizeof(struct airtime_stats));
    _small_stats_src_tab[_current_small_stats].clear();

    //INIT: clear "last" stats
    memset(&(_small_stats[SMALL_STATS_SIZE-1]),0,sizeof(struct airtime_stats));
    _small_stats_src_tab[SMALL_STATS_SIZE-1].clear();
  }

  return 0;
}

void
ChannelStats::run_timer(Timer *)
{
  if ( _proc_read && (_stats_interval == _proc_interval) ) {  //if _stats_interval equals _proc_interval, then the _stats_timer
    BRN_DEBUG("Read proc");                                   //is used to read the procfile if this is requested
    proc_read();
  } else {
    BRN_DEBUG("Don't read proc");
  }

  if ( ! _enable_full_stats ) {                               //if full_stats are not requested, the calc stats now

    calc_stats_final(&(_small_stats[_current_small_stats]), &(_small_stats_src_tab[_current_small_stats]), _stats_interval);

    _small_stats[_current_small_stats].stats_id = _stats_id++;

    _current_small_stats = (_current_small_stats + 1) % SMALL_STATS_SIZE;

    //TODO: don't clear. take info (e.g. last seq number) for next round
    memset(&(_small_stats[_current_small_stats]),0,sizeof(struct airtime_stats));
    _small_stats_src_tab[_current_small_stats].clear();

    _small_stats_rssiinfo[_current_small_stats].reset();
  }

  if ( _stats_interval > 0 ) _stats_timer.reschedule_after_msec(_stats_interval);
}

void
ChannelStats::static_proc_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
 (reinterpret_cast<ChannelStats*>(f))->proc_read();
}

void
ChannelStats::proc_read()
{
  if ( _proc_read ) {
    readProcHandler();
    if ( _stats_interval != _proc_interval ) _proc_timer.schedule_after_msec(_proc_interval);
  }
}

/*********************************************/
/************* RX BASED STATS ****************/
/*********************************************/
void
ChannelStats::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  struct airtime_stats *small_stats = &(_small_stats[_current_small_stats]);

  struct click_wifi *w = (struct click_wifi *) p->data();

  //click_chatter("Duration: %d", w->i_dur);
  int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;
  uint16_t seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
  uint32_t p_length = p->length();

  EtherAddress src;
  switch (type) {
    case WIFI_FC0_TYPE_CTL:
      src = brn_etheraddress_broadcast;
      seq = 0;
      break;
    case WIFI_FC0_TYPE_DATA:
    case WIFI_FC0_TYPE_MGT:
    default:
      src = EtherAddress(w->i_addr2);
      break;
  }
  EtherAddress dst = EtherAddress(w->i_addr1);

  /* General stuff */
  _channel = BRNPacketAnno::channel_anno(p);
  if ( _device != NULL && _channel != 0 ) _device->setChannel(_channel);

  if ( !_enable_full_stats ) {
    small_stats->last = p->timestamp_anno();
  }

  /* Handle TXFeedback */
  if ( ceh->flags & WIFI_EXTRA_TX ) {
    int tx_count = BrnWifi::get_data_sent_count(ceh);

    //BRN_Info("TxCount: %d",tx_count);

    for ( int i = 0; i < tx_count; i++ ) {
      int t0,t1,t2,t3;
      uint8_t rate_is_ht, rate_index, rate_bw, rate_sgi;
      rate_is_ht = rate_index = rate_bw = rate_sgi = 0;

      t0 = ceh->max_tries + 1;
      t1 = t0 + ceh->max_tries1;
      t2 = t1 + ceh->max_tries2;
      t3 = t2 + ceh->max_tries3;

      uint16_t rate = 0;
      if ( i < t0 ) {
        if ( BrnWifi::getMCS(ceh,0) == 1) {
          BrnWifi::toMCS(&rate_index, &rate_bw, &rate_sgi, ceh->rate);
          rate_is_ht = 1;
        } else {
          rate = ceh->rate;
          rate_is_ht = 0;
        }
      } else if ( i < t1 ) {
        if ( BrnWifi::getMCS(ceh,1) == 1 ) {
          BrnWifi::toMCS(&rate_index, &rate_bw, &rate_sgi, ceh->rate1);
          rate_is_ht = 1;
        } else {
          rate = ceh->rate1;
          rate_is_ht = 0;
        }
      } else if ( i < t2 ) {
        if ( BrnWifi::getMCS(ceh,2) == 1 ) {
          BrnWifi::toMCS(&rate_index, &rate_bw, &rate_sgi, ceh->rate2);
          rate_is_ht = 1;
        } else {
          rate = ceh->rate2;
          rate_is_ht = 0;
        }
      } else if ( i < t3 ) {
        if ( BrnWifi::getMCS(ceh,3) == 1 ) {
          BrnWifi::toMCS(&rate_index, &rate_bw, &rate_sgi, ceh->rate3);
          rate_is_ht = 1;
        } else {
          rate = ceh->rate3;
          rate_is_ht = 0;
        }
      }

      if (_enable_full_stats) {

        PacketInfo *new_pi = new PacketInfo();
        new_pi->_src = src;
        new_pi->_dst = dst;
        new_pi->_rx_time = p->timestamp_anno();
        new_pi->_length = p_length+4;
        new_pi->_foreign = false;
        new_pi->_channel = _channel;
        new_pi->_rx = false;
        new_pi->_rate = rate;
        new_pi->_seq = seq;

        new_pi->_is_ht_rate = (rate_is_ht == 1);

        if (rate != 0) {
          if ( rate_is_ht )
            new_pi->_duration = BrnWifi::pkt_duration(p_length + 4 /*crc*/, rate_index, rate_bw, rate_sgi);
          else
            new_pi->_duration = calc_transmit_time(rate, p_length + 4 /*crc*/); //TODO: check CRC ??

          new_pi->_retry = (i > 0);
          new_pi->_unicast = !dst.is_broadcast();
        } else {
          new_pi->_duration = 0;
        }

        _packet_list.push_back(new_pi);

      } else {// small: incremental update
        if (rate != 0) {
          if (dst.is_broadcast())
            small_stats->tx_bcast_packets++;
          else {
            small_stats->tx_ucast_packets++;
            if ( i > 0 ) small_stats->tx_retry_packets++;
          }
          if ( rate_is_ht )
            small_stats->duration_tx += BrnWifi::pkt_duration(p_length + 4 /*crc*/, rate_index, rate_bw, rate_sgi);
          else
            small_stats->duration_tx += calc_transmit_time(rate, p_length + 4 /*crc*/); // update duration counter TODO: check CRC ??

          small_stats->txpackets++;
          small_stats->tx_bytes += p_length + 4;
        } else {
          small_stats->zero_rate_packets++;
        }
      }
    }

    /** add rts */
    /** TODO: what is the basic rate ?? */
    if ( (!dst.is_broadcast()) && ((ceh->flags & WIFI_EXTRA_DO_RTS_CTS) != 0)) {
      int rts_count = BrnWifi::get_rts_sent_count(ceh);

      for (int i = 0; i < rts_count; i++) {
        if (_enable_full_stats) {

          PacketInfo *new_pi = new PacketInfo();
          new_pi->_src = dst;
          new_pi->_dst = src;
          new_pi->_rx_time = p->timestamp_anno();
          new_pi->_length = 16;
          new_pi->_foreign = false;
          new_pi->_channel = _channel;
          new_pi->_rx = false;
          new_pi->_rate = 2; //TODO: what is the basic rate
          new_pi->_seq = seq;

          new_pi->_is_ht_rate = false;

          new_pi->_duration = calc_transmit_time(2, 16);

          new_pi->_retry = false;
          new_pi->_unicast = true;

          _packet_list.push_back(new_pi);
        } else {
          small_stats->tx_ucast_packets++;
          small_stats->duration_tx += calc_transmit_time(2, 16);
          small_stats->txpackets++;
          small_stats->tx_bytes += 16;
        }
      }
    }
  } else {   /* Handle Rx Packets */

    PacketInfo *new_pi = NULL;
    bool is_ht_rate = (BrnWifi::getMCS(ceh,0) == 1);

    if ( _enable_full_stats ) {
      new_pi = new PacketInfo();
      new_pi->_rx_time = p->timestamp_anno();
      new_pi->_length = p_length + 4;
      new_pi->_foreign = true;
      new_pi->_channel = _channel;
      new_pi->_rx = true;
      new_pi->_rate = ceh->rate;
      new_pi->_is_ht_rate = is_ht_rate;
      new_pi->_seq = seq;
    }

    if ( (ceh->rate != 0) || is_ht_rate ) { //has valid rate

      uint32_t duration;

      if (is_ht_rate) {
        uint8_t  rate_index, rate_bw, rate_sgi;
        BrnWifi::toMCS(&rate_index, &rate_bw, &rate_sgi, ceh->rate);
        duration = BrnWifi::pkt_duration(p_length + 4 /*crc*/, rate_index, rate_bw, rate_sgi);
      } else
        duration = calc_transmit_time(ceh->rate, p_length + 4 /*crc*/); //calc duration TODO: check CRC ??

      uint8_t state = STATE_UNKNOWN;

      bool retry = (w->i_fc[1] & WIFI_FC1_RETRY);

      if ( ceh->magic == WIFI_EXTRA_MAGIC ) {
        if ((ceh->flags & WIFI_EXTRA_RX_ERR) == 0 ) state = STATE_OK;
        else {
          if  (ceh->flags & WIFI_EXTRA_RX_CRC_ERR) state = STATE_CRC;
          else if (ceh->flags & WIFI_EXTRA_RX_PHY_ERR) state = STATE_PHY;
          else state = STATE_ERROR;
        }
      }

      if (_enable_full_stats) {
        new_pi->_duration = duration;

        new_pi->_src = src;
        new_pi->_dst = dst;
        new_pi->_state = state;

        new_pi->_noise = (signed char)ceh->silence;
        new_pi->_rssi = (signed char)ceh->rssi;

        new_pi->_retry = retry;
        new_pi->_unicast = !dst.is_broadcast();

        if ( state == STATE_OK ) new_pi->_nav = w->i_dur;
        else new_pi->_nav = 0;

      } else { // fast mode
        // update duration counter
        small_stats->duration_rx += duration;
        small_stats->rxpackets++;
        small_stats->rx_bytes += p_length + 4;

        int silence = (signed char)ceh->silence;
        small_stats->avg_noise += silence;
        small_stats->std_noise += (silence * silence);
        int rssi = (signed char)ceh->rssi;

        small_stats->avg_rssi += rssi;
        small_stats->std_rssi += (rssi * rssi);

        /* collect information about rssi, needed to decode a packet correctly(STATE_OK) */
        if ( state == STATE_OK ) {
          _small_stats_rssiinfo[_current_small_stats].add(ceh->rate,is_ht_rate,rssi);
          _rssiinfo_sum.add(ceh->rate,is_ht_rate,rssi);
        }

        bool has_ext_rx_status = BrnWifi::hasExtRxStatus(ceh);
        struct brn_click_wifi_extra_rx_status *ext_rx_status = NULL;

        if ( has_ext_rx_status ) {
          ext_rx_status = (struct brn_click_wifi_extra_rx_status *)BRNPacketAnno::get_brn_wifi_extra_rx_status_anno(p);
          small_stats->avg_ctl_rssi[0] += ext_rx_status->rssi_ctl[0];
          small_stats->avg_ctl_rssi[1] += ext_rx_status->rssi_ctl[1];
          small_stats->avg_ctl_rssi[2] += ext_rx_status->rssi_ctl[2];

          small_stats->avg_ext_rssi[0] += ext_rx_status->rssi_ext[0];
          small_stats->avg_ext_rssi[1] += ext_rx_status->rssi_ext[1];
          small_stats->avg_ext_rssi[2] += ext_rx_status->rssi_ext[2];
        }

        switch (state) {
          case STATE_OK:
                small_stats->duration_noerr_rx += duration;
                small_stats->noerr_packets++;
                small_stats->nav += w->i_dur;
                break;
          case STATE_CRC:
                small_stats->duration_crc_rx += duration;
                small_stats->crc_packets++;
                break;
          case STATE_PHY:
                small_stats->duration_phy_rx += duration;
                small_stats->phy_packets++;
                break;
          case STATE_ERROR:
                small_stats->duration_unknown_err_rx += duration;
                small_stats->unknown_err_packets++;
                break;
        }

        if (dst.is_broadcast()) small_stats->rx_bcast_packets++;
        else {
          small_stats->rx_ucast_packets++;
          if (retry) small_stats->rx_retry_packets++;
        }

        if ( _neighbour_stats ) {
          if ( state == STATE_OK ) {
            SrcInfo *src_info = NULL;
            SrcInfo **src_info_p;
            if ( (src_info_p = _small_stats_src_tab[_current_small_stats].findp(src)) == NULL ) {       //search in tab
              if ( (src_info_p = _small_stats_src_tab_pool[_current_small_stats].findp(src)) == NULL ) {//search in pool
              //insert new ( rssi value is set via constructor)
                src_info = new SrcInfo(rssi,p_length + 4, duration, w->i_dur, seq);     //create new
                _small_stats_src_tab_pool[_current_small_stats].insert(src, src_info);  //insert pool
              } else {
                src_info = *src_info_p;
                src_info->reset();                                                      //clear found
                src_info->set(rssi,p_length + 4, duration, w->i_dur, seq);              //set
              }
              _small_stats_src_tab[_current_small_stats].insert(src, src_info);         //insert tab
            } else {
                src_info = *src_info_p;
                src_info->add_packet_info(rssi,p_length + 4, duration, w->i_dur, seq);
            }

            if (has_ext_rx_status) src_info->add_ctl_ext_rssi(ext_rx_status);
          }
        }
      }
    } else { //RX with rate = 0
      if (_enable_full_stats)
        new_pi->_duration = 0;
      else
        small_stats->zero_rate_packets++;
    }

    if (_enable_full_stats) {
      clear_old();
      _packet_list.push_back(new_pi);
    }

    if ( ((type == WIFI_FC0_TYPE_DATA) ||
         ((type == WIFI_FC0_TYPE_CTL) && ((w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) == WIFI_FC0_SUBTYPE_RTS ))) &&
         (dst == *(_device->getEtherAddress())) ) {
      if (_enable_full_stats) {
        PacketInfo *new_pi = new PacketInfo();
        new_pi->_src = dst;
        new_pi->_dst = src;
        new_pi->_rx_time = p->timestamp_anno();
        new_pi->_length = 10;
        new_pi->_foreign = false;
        new_pi->_channel = _channel;
        new_pi->_rx = false;
        new_pi->_rate = 2; //TODO: what is the basic rate
        new_pi->_seq = seq;

        new_pi->_is_ht_rate = false;

        new_pi->_duration = calc_transmit_time(2, 10);

        new_pi->_retry = false;
        new_pi->_unicast = true;

        _packet_list.push_back(new_pi);
      } else {
        small_stats->tx_ucast_packets++;
        small_stats->duration_tx += calc_transmit_time(2, 10);
        small_stats->txpackets++;
        small_stats->tx_bytes += 10;
      }
    }

  }

  output(port).push(p);
}

void
ChannelStats::clear_old()
{
  Timestamp now = Timestamp::now();
  Timestamp diff;

  if ( _packet_list.size() == 0 ) return;

  int i;
  for ( i = 0; i < _packet_list.size(); i++) {
    PacketInfo *pi = _packet_list[i];
    diff = now - pi->_rx_time;
    if ( diff.msecval() <= _save_duration ) break; //TODO: exact calc
    else delete pi;
  }

  if ( i > 0 ) _packet_list.erase(_packet_list.begin(), _packet_list.begin() + i);
}


/*********************************************/
/************* HW BASED STATS ****************/
/*********************************************/
void
ChannelStats::addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx, uint32_t cycles, uint32_t busy_cycles, uint32_t rx_cycles, uint32_t tx_cycles) {
  PacketInfoHW *new_pi = new PacketInfoHW();
  new_pi->_time = *time;

  if ( _packet_list_hw.size() == 0 ) { //we don't care about the first hw info, since there is no time ref point
    new_pi->_busy = new_pi->_rx = new_pi->_tx = 0;
    new_pi->_cycles = new_pi->_busy_cycles = new_pi->_rx_cycles = new_pi->_tx_cycles = 0;
  } else {
    uint32_t diff = (new_pi->_time - _packet_list_hw[_packet_list_hw.size()-1]->_time).usecval();
    new_pi->_busy = (diff * busy) / 100;
    new_pi->_rx = (diff * rx) / 100;
    new_pi->_tx = (diff * tx) / 100;
    new_pi->_cycles = cycles;
    new_pi->_busy_cycles = busy_cycles;
    new_pi->_rx_cycles = rx_cycles;
    new_pi->_tx_cycles = tx_cycles;
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

  int i;
  for ( i = 0; i < _packet_list_hw.size(); i++) {
    PacketInfoHW *pi = _packet_list_hw[i];
    diff = now - pi->_time;
    if ( diff.msecval() <= _save_duration ) break; //TODO: exact calc
    else delete pi;
  }

  if ( i > 0 ) _packet_list_hw.erase(_packet_list_hw.begin(), _packet_list_hw.begin() + i);
}

/*************** PROC HANDLER ****************/

void
ChannelStats::readProcHandler()
{
  int busy, rx, tx;
  uint32_t hw_cycles, busy_cycles, rx_cycles, tx_cycles;

#if CLICK_NS
  int stats[16];
  simclick_sim_command(router()->simnode(), SIMCLICK_GET_PERFORMANCE_COUNTER, &stats);

  busy = stats[0];
  rx = stats[1];
  tx = stats[2];

  hw_cycles = stats[3];
  busy_cycles = stats[4];
  rx_cycles = stats[5];
  tx_cycles = stats[6];

#else
  String raw_info = file_string(_proc_file);
  Vector<String> args;

  cp_spacevec(raw_info, args);

  if ( args.size() <= 6 ) return;

  cp_integer(args[1],&busy);
  cp_integer(args[3],&rx);
  cp_integer(args[5],&tx);
  cp_integer(args[7],&hw_cycles);
  cp_integer(args[9],&busy_cycles);
  cp_integer(args[11],&rx_cycles);
  cp_integer(args[13],&tx_cycles);

#endif
  Timestamp now = Timestamp::now();

  if ( _enable_full_stats ) {
    addHWStat(&now, busy, rx, tx, hw_cycles, busy_cycles, rx_cycles, tx_cycles);
  } else {
    _small_stats[_current_small_stats].last_hw = now;
    _small_stats[_current_small_stats].hw_busy += busy;
    _small_stats[_current_small_stats].hw_rx += rx;
    _small_stats[_current_small_stats].hw_tx += tx;
    _small_stats[_current_small_stats].hw_count++;
    _small_stats[_current_small_stats].hw_cycles += hw_cycles;
    _small_stats[_current_small_stats].hw_busy_cycles += busy_cycles;
    _small_stats[_current_small_stats].hw_rx_cycles += rx_cycles;
    _small_stats[_current_small_stats].hw_tx_cycles += tx_cycles;
  }
}

/*********************************************/
/************ CALCULATE STATS ****************/
/*********************************************/

void
ChannelStats::get_stats(struct airtime_stats *cstats, int /*time*/) {
  calc_stats(cstats, NULL, NULL);
}

void
ChannelStats::calc_stats(struct airtime_stats *cstats, SrcInfoTable *src_tab, RSSIInfo *rssi_tab)
{
  Timestamp now = Timestamp::now();
  Timestamp diff;
  int i = 0;

  HashMap<EtherAddress, EtherAddress> sources;

  diff = now - cstats->last_update;

  if ( diff.usecval() <= _min_update_time * 1000 ) return;

  if ( src_tab ) {
    BRN_ERROR("Clear SRCTAB");
    for (SrcInfoTableIter iter = src_tab->begin(); iter.live(); ++iter) delete iter.value();
    src_tab->clear();
  }
  if ( rssi_tab ) rssi_tab->reset();

  memset(cstats, 0, sizeof(struct airtime_stats));

  cstats->stats_id = _stats_id++;
  cstats->duration = _stats_interval;
  cstats->last_update = now;

  if ( _packet_list.size() == 0 ) return;

  PacketInfo *pi = _packet_list[0];
  diff = now - pi->_rx_time;

  for ( i = 0; i < _packet_list.size(); i++) {
    pi = _packet_list[i];
    diff = now - pi->_rx_time;
    if ( diff.msecval() <= _stats_interval )  break; //TODO: exact calc
  }

  for (; i < _packet_list.size(); i++) {
    pi = _packet_list[i];

    if ( pi->_rx) {
      if ( pi->_rate > 0 ) {
        cstats->duration_rx += pi->_duration;
        cstats->avg_noise += pi->_noise;
        cstats->std_noise += (pi->_noise*pi->_noise);
        cstats->avg_rssi += pi->_rssi;
        cstats->std_rssi += (pi->_rssi*pi->_rssi);
        cstats->nav += pi->_nav;
        cstats->rxpackets++;

        switch (pi->_state) {
          case STATE_OK:
            cstats->duration_noerr_rx += pi->_duration;
            cstats->noerr_packets++;
            break;
          case STATE_CRC:
            cstats->duration_crc_rx += pi->_duration;
            cstats->crc_packets++;
            break;
          case STATE_PHY:
            cstats->duration_phy_rx += pi->_duration;
            cstats->phy_packets++;
            break;
          case STATE_ERROR:
            cstats->duration_unknown_err_rx += pi->_duration;
            cstats->unknown_err_packets++;
            break;

        }

        if (! pi->_unicast) cstats->rx_bcast_packets++;
        else {
          cstats->rx_ucast_packets++;
          if (pi->_retry) cstats->rx_retry_packets++;
        }

        if ( pi->_state == STATE_OK ) {
          if ( sources.findp(pi->_src) == NULL ) sources.insert(pi->_src,pi->_src);

          if ( src_tab != NULL ) {
            SrcInfo **src_i;
            if ( (src_i = src_tab->findp(pi->_src)) == NULL ) {
              src_tab->insert(pi->_src, new SrcInfo((uint32_t)pi->_rssi,(uint32_t)pi->_length, pi->_duration, pi->_nav, pi->_seq));
            } else {
              (*src_i)->add_packet_info((uint32_t)pi->_rssi,(uint32_t)pi->_length, pi->_duration, pi->_nav, pi->_seq);
            }
          }
        }

        if ( rssi_tab != NULL ) rssi_tab->add(pi->_rate, pi->_is_ht_rate, pi->_rssi);
      } else {
        cstats->zero_rate_packets++;
      }
    } else {  //tx packets
      if ( pi->_rate > 0 ) {
        if (! pi->_unicast) cstats->tx_bcast_packets++;
        else {
          cstats->tx_ucast_packets++;
          if (pi->_retry) cstats->tx_retry_packets++;
        }

        cstats->duration_tx += pi->_duration;
        cstats->txpackets++;
      } else {
        cstats->zero_rate_packets++;
      }
    }
  }

  calc_stats_final(cstats, src_tab, _stats_interval);

  cstats->last = _packet_list[_packet_list.size()-1]->_rx_time;

  cstats->no_sources = sources.size();

  /******** HW ***********/

  cstats->hw_available = (_packet_list_hw.size() != 0);
  if ( _packet_list_hw.size() == 0 ) return; //no hw stats available

  PacketInfoHW *pi_hw = _packet_list_hw[0];
  diff = now - pi_hw->_time;

  for ( i = 0; i < _packet_list_hw.size(); i++) {
    pi_hw = _packet_list_hw[i];
    diff = now - pi_hw->_time;
    if ( diff.msecval() <= _stats_interval )  break; //TODO: exact calc
  }

  for (; i < _packet_list_hw.size(); i++) {
    pi_hw = _packet_list_hw[i];
    cstats->hw_busy += pi_hw->_busy;
    cstats->hw_rx += pi_hw->_rx;
    cstats->hw_tx += pi_hw->_tx;
    cstats->hw_cycles += pi_hw->_cycles;
    cstats->hw_busy_cycles += pi_hw->_busy_cycles;
    cstats->hw_rx_cycles += pi_hw->_rx_cycles;
    cstats->hw_tx_cycles += pi_hw->_tx_cycles;
  }

  int diff_time = 10 * _stats_interval;

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
  _packet_list_hw.clear();
  _rssiinfo_sum.reset();
}

/**************************************************************************************************************/
/****************************** CALC STATS FINAL (use for small and full stats) *******************************/
/**************************************************************************************************************/

void
ChannelStats::calc_stats_final(struct airtime_stats *small_stats, SrcInfoTable *src_tab, int duration)
{
  int diff_time = duration * 10;

  small_stats->duration_busy = small_stats->frac_mac_busy = small_stats->duration_rx + small_stats->duration_tx;
  small_stats->last_update = Timestamp::now();
  small_stats->frac_mac_busy /= diff_time;
  small_stats->frac_mac_rx = small_stats->duration_rx / diff_time;
  small_stats->frac_mac_tx = small_stats->duration_tx / diff_time;
  small_stats->frac_mac_noerr_rx = small_stats->duration_noerr_rx / diff_time;
  small_stats->frac_mac_crc_rx = small_stats->duration_crc_rx / diff_time;
  small_stats->frac_mac_phy_rx = small_stats->duration_phy_rx / diff_time;
  small_stats->frac_mac_unknown_err_rx = small_stats->duration_unknown_err_rx / diff_time;

  small_stats->duration = duration;

  if ( small_stats->frac_mac_busy > 100 ) {
    BRN_ERROR("Mac Busy: Overflow: %d", small_stats->frac_mac_busy);
    small_stats->frac_mac_busy = 100;

    if ( small_stats->frac_mac_rx > 100 ) {
      small_stats->frac_mac_rx = 100;
      if ( small_stats->frac_mac_noerr_rx > 100 ) small_stats->frac_mac_noerr_rx = 100;
      if ( small_stats->frac_mac_crc_rx > 100 ) small_stats->frac_mac_crc_rx = 100;
      if ( small_stats->frac_mac_phy_rx > 100 ) small_stats->frac_mac_phy_rx = 100;
      if ( small_stats->frac_mac_unknown_err_rx > 100 ) small_stats->frac_mac_unknown_err_rx = 100;
    }
    if ( small_stats->frac_mac_tx > 100 ) small_stats->frac_mac_tx = 100;
  }

  if ( small_stats->rxpackets > 0 ) {
    small_stats->avg_noise /= (int32_t)small_stats->rxpackets;
    if ( small_stats->avg_noise > 120 ) small_stats->avg_noise = 120;
    else if ( small_stats->avg_noise < -120 ) small_stats->avg_noise = -120;

    small_stats->std_noise = isqrt32((small_stats->std_noise/(int32_t)small_stats->rxpackets) - (small_stats->avg_noise*small_stats->avg_noise));

    small_stats->avg_rssi /= (int32_t)small_stats->rxpackets;
    if ( small_stats->avg_rssi > 120 ) small_stats->avg_rssi = 120;
    else if ( small_stats->avg_rssi < -120 ) small_stats->avg_rssi = -120;

    small_stats->std_rssi = isqrt32((small_stats->std_rssi/(int32_t)small_stats->rxpackets) - (small_stats->avg_rssi*small_stats->avg_rssi));

    small_stats->avg_ctl_rssi[0] /= (int32_t)small_stats->rxpackets;
    small_stats->avg_ctl_rssi[1] /= (int32_t)small_stats->rxpackets;
    small_stats->avg_ctl_rssi[2] /= (int32_t)small_stats->rxpackets;
    small_stats->avg_ext_rssi[0] /= (int32_t)small_stats->rxpackets;
    small_stats->avg_ext_rssi[1] /= (int32_t)small_stats->rxpackets;
    small_stats->avg_ext_rssi[2] /= (int32_t)small_stats->rxpackets;

  } else {
    small_stats->avg_noise = -100; // default value
    small_stats->std_noise = 0;
    small_stats->avg_rssi = 0;
    small_stats->std_rssi = 0;
  }

  if ( small_stats->hw_count > 0 ) {
    small_stats->hw_available = true;
    small_stats->hw_busy /= small_stats->hw_count;
    small_stats->hw_rx /= small_stats->hw_count;
    small_stats->hw_tx /= small_stats->hw_count;
  }

  if ( src_tab ) {

    small_stats->no_sources = src_tab->size();

    for (SrcInfoTableIter iter = src_tab->begin(); iter.live(); ++iter)
      iter.value()->calc_values();

  }
}

/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum {H_RESET, H_MAX_TIME, H_STATS, H_STATS_XML };

String
ChannelStats::stats_handler(int mode)
{
  StringAccum sa;
  sa.reserve(4096);

  struct airtime_stats *stats;
  SrcInfoTable          *src_tab;
  RSSIInfo              *rssi_info;

  if ( _enable_full_stats ) {
    if (_neighbour_stats)
      calc_stats(&_full_stats, &_full_stats_srcinfo_tab, &_full_stats_rssiinfo);
    else
      calc_stats(&_full_stats, NULL, &_full_stats_rssiinfo);

    stats = &_full_stats;
    src_tab = &_full_stats_srcinfo_tab;
    rssi_info = &_full_stats_rssiinfo;
  } else {
    stats = get_latest_stats();
    src_tab = get_latest_stats_neighbours();
    rssi_info = get_latest_rssi_info();
  }

  switch (mode) {
    case H_STATS:
    case H_STATS_XML:

      sa << "<channelstats";

      if ( _device )
        sa << " node=\"" << _device->getEtherAddress()->unparse() << "\" time=\"" << stats->last_update.unparse();
      else
        sa << " node=\"" << BRN_NODE_NAME << "\" time=\"" << stats->last_update.unparse();

        sa << "\" id=\"" << stats->stats_id << "\" length=\"" << stats->duration;

      if ( _proc_read ) sa << "\" hw_duration=\"" << _proc_interval;
      else              sa << "\" hw_duration=\"0";

      sa << "\" unit=\"ms\" >\n";

      sa << "\t<mac packets=\"" << (stats->rxpackets+stats->txpackets) << "\" rx_pkt=\"" << stats->rxpackets;
      sa << "\" no_err_pkt=\"" << stats->noerr_packets << "\" crc_err_pkt=\"" << stats->crc_packets;
      sa << "\" phy_err_pkt=\"" << stats->phy_packets << "\" unknown_err_pkt=\"" << stats->unknown_err_packets;
      sa << "\" tx_pkt=\"" << stats->txpackets;
      sa << "\" rx_unicast_pkt=\"" << stats->rx_ucast_packets << "\" rx_retry_pkt=\"" << stats->rx_retry_packets;
      sa << "\" rx_bcast_pkt=\"" << stats->rx_bcast_packets << "\" rx_bytes=\"" << stats->rx_bytes;
      sa << "\" tx_unicast_pkt=\"" << stats->tx_ucast_packets << "\" tx_retry_pkt=\"" << stats->tx_retry_packets;
      sa << "\" tx_bcast_pkt=\"" << stats->tx_bcast_packets << "\" tx_bytes=\"" << stats->tx_bytes;
      sa << "\" zero_err_pkt=\"" << stats->zero_rate_packets;
      sa << "\" last_packet_time=\"" << stats->last.unparse();
      sa << "\" no_src=\"" << stats->no_sources << "\" />\n";

      sa << "\t<mac_percentage busy=\"" << stats->frac_mac_busy << "\" rx=\"" << stats->frac_mac_rx;
      sa << "\" tx=\"" << stats->frac_mac_tx << "\" noerr_rx=\"" << stats->frac_mac_noerr_rx;
      sa << "\" crc_rx=\"" << stats->frac_mac_crc_rx << "\" phy_rx=\"" << stats->frac_mac_phy_rx;
      sa << "\" unknown_err_rx=\"" << stats->frac_mac_unknown_err_rx << "\" unit=\"percent\" />\n";

      sa << "\t<mac_duration busy=\"" << stats->duration_busy << "\" rx=\"" << stats->duration_rx;
      sa << "\" tx=\"" << stats->duration_tx << "\" noerr_rx=\"" << stats->duration_noerr_rx;
      sa << "\" crc_rx=\"" << stats->duration_crc_rx << "\" phy_rx=\"" << stats->duration_phy_rx;
      sa << "\" unknown_err_rx=\"" << stats->duration_unknown_err_rx << "\" unit=\"us\" />\n";

      sa << "\t<mac_virtual nav=\"" << stats->nav << "\" unit=\"us\" />\n";

      sa << "\t<phy hwbusy=\"" << stats->hw_busy << "\" hwrx=\"" << stats->hw_rx << "\" hwtx=\"" << stats->hw_tx;
      sa << "\" last_hw_stat_time=\"" << stats->last_hw.unparse() << "\" hw_stats_count=\"" << stats->hw_count;
      sa << "\" avg_noise=\"" << stats->avg_noise << "\" std_noise=\"" << stats->std_noise;
      sa << "\" avg_rssi=\"" << stats->avg_rssi << "\" std_rssi=\"" << stats->std_rssi;
      sa << "\" ctl_rssi0=\"" << stats->avg_ctl_rssi[0] << "\" ctl_rssi1=\"" << stats->avg_ctl_rssi[1] << "\" ctl_rssi2=\"" << stats->avg_ctl_rssi[2];
      sa << "\" ext_rssi0=\"" << stats->avg_ext_rssi[0] << "\" ext_rssi1=\"" << stats->avg_ext_rssi[1] << "\" ext_rssi2=\"" << stats->avg_ext_rssi[2];
      sa << "\" channel=\"";
      if ( _device ) sa << (int)(_device->getChannel());
      else           sa << _channel;

      sa << "\" />\n\t<perf_counter cycles=\"" << stats->hw_cycles << "\" busy_cycles=\"" << stats->hw_busy_cycles;
      sa << "\" rx_cycles=\"" << stats->hw_rx_cycles << "\" tx_cycles=\"" << stats->hw_tx_cycles;

      sa << "\" />\n\t<neighbourstats>\n";
      for (SrcInfoTableIter iter = src_tab->begin(); iter.live(); ++iter) {
        SrcInfo *src = iter.value();
        EtherAddress ea = iter.key();
        sa << "\t\t<nb addr=\"" << ea.unparse() << "\" rssi=\"" << src->_avg_rssi << "\" std_rssi=\"";
        sa  << src->_std_rssi << "\" min_rssi=\"" << src->_min_rssi << "\" max_rssi=\"" << src->_max_rssi;
        sa << "\" pkt_cnt=\"" << src->_pkt_count << "\" rx_bytes=\"" << src->_byte_count;
        sa << "\" duration=\"" << src->_duration << "\" nav=\"" << src->_nav << "\" last_seq=\"" << src->_last_seq;
	sa << "\" missed_seq=\"" << src->_missed_seq << "\" >\n";
        sa << "\t\t\t<rssi_extended>\n\t\t\t\t<ctl rssi0=\"" << src->_avg_ctl_rssi[0];
        sa << "\" rssi1=\"" << src->_avg_ctl_rssi[1] << "\" rssi2=\"" << src->_avg_ctl_rssi[2] << "\" />\n\t\t\t\t<ext rssi0=\"";
        sa << src->_avg_ext_rssi[0] << "\" rssi1=\"" << src->_avg_ext_rssi[1] << "\" rssi2=\"" << src->_avg_ext_rssi[2] << "\" />\n";
        sa << "\t\t\t</rssi_extended>\n\t\t\t<rssi_hist size=\"" << src->_rssi_hist_index << "\" max_size=\"" << src->_rssi_hist_size;
        sa << "\" overflow=\"" << src->_rssi_hist_overflow << "\" values=\"";
        for ( uint32_t rssi_i = 0; rssi_i < src->_rssi_hist_index; rssi_i++) {
          if ( rssi_i > 0 ) sa << ",";
          sa << (uint32_t)(src->_rssi_hist[rssi_i]);
        }
        sa << "\" />\n\t\t</nb>\n";
      }
      sa << "\t</neighbourstats>\n\t<rssi_stats min_rssi=\"" << (uint32_t)rssi_info->min_rssi << "\" >\n";

      for ( int i = 0; i <= 255; i++ ) {
        if ( rssi_info->min_rssi_per_rate[i] != 255 ) {
          sa << "\t\t<rssi_for_rate rate=\"" << i << "\" min_rssi=\"" << (uint32_t)rssi_info->min_rssi_per_rate[i] << "\" />\n";
        }
      }

      sa << "\t</rssi_stats>\n\t<rssi_stats_global min_rssi=\"" << (uint32_t)_rssiinfo_sum.min_rssi << "\" >\n";

      for ( int i = 0; i <= 255; i++ ) {
        if ( _rssiinfo_sum.min_rssi_per_rate[i] != 255 ) {
          sa << "\t\t<rssi_for_rate rate=\"" << i << "\" min_rssi=\"" << (uint32_t)_rssiinfo_sum.min_rssi_per_rate[i] << "\" />\n";
        }
      }

      sa << "\t</rssi_stats_global>\n";

#ifdef CLICK_NS
      struct rx_tx_stats rxtxstats;
      simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_GET_RXTXSTATS, &rxtxstats);

      sa << "\t<sim_stats>\n\t\t<txstats rts=\"" << rxtxstats.tx_no_rts_ << "\" cts=\"" << rxtxstats.tx_no_cts_;
      sa << "\" data=\"" << rxtxstats.tx_no_data_ << "\" broadcast=\"" << rxtxstats.tx_no_bcast_;
      sa << "\" unicast=\"" << rxtxstats.tx_no_unic_ << "\" ack=\"" << rxtxstats.tx_no_ack_ << "\" />\n";
      sa << "\t\t<rxstats rts=\"" << rxtxstats.rx_no_rts_ << "\" cts=\"" << rxtxstats.rx_no_cts_;
      sa << "\" data=\"" << rxtxstats.rx_no_data_ << "\" broadcast=\"" << rxtxstats.rx_no_bcast_;
      sa << "\" unicast=\"" << rxtxstats.rx_no_unic_ << "\" ack=\"" << rxtxstats.rx_no_ack_ << "\" />\n";
      sa << "\t\t<packetloss nodecol=\"" << rxtxstats.no_nodes_col_ << "\" packetcol=\"";
      sa << rxtxstats.no_packets_col_ << "\" capture=\"" << rxtxstats.no_cap_ << "\" />\n\t</sim_stats>\n";
#endif

      sa << "</channelstats>\n";

  }
  return sa.take_string();
}

static String
ChannelStats_read_param(Element *e, void *thunk)
{
  ChannelStats *td = reinterpret_cast<ChannelStats *>(e);
  switch ((uintptr_t) thunk) {
    case H_STATS:
    case H_STATS_XML:
      return td->stats_handler((uintptr_t) thunk);
      break;
    case H_MAX_TIME:
      return String(td->_stats_interval) + "\n";
  }

  return String();
}

static int
ChannelStats_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  ChannelStats *f = reinterpret_cast<ChannelStats *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_MAX_TIME: {
      int mt;
      if (!cp_integer(s, &mt))
        return errh->error("max time parameter must be integer");
      f->_stats_interval = mt;
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
  BRNElement::add_handlers();

  add_read_handler("max_time", ChannelStats_read_param, (void *) H_MAX_TIME);
  add_read_handler("stats", ChannelStats_read_param, (void *) H_STATS);
  add_read_handler("stats_xml", ChannelStats_read_param, (void *) H_STATS_XML);

  add_write_handler("reset", ChannelStats_write_param, (void *) H_RESET, Handler::BUTTON);
  add_write_handler("max_time", ChannelStats_write_param, (void *) H_MAX_TIME);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ChannelStats)
ELEMENT_MT_SAFE(ChannelStats)
