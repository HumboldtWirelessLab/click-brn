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
#include <elements/brn2/wifi/brnwifi.hh>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>

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

  click_random_srandom();

  _stats_timer.initialize(this);
  _proc_timer.initialize(this);

  if ( (!_enable_full_stats) || (_proc_read && (_stats_interval == _proc_interval)) )
    _stats_timer.schedule_after_msec(_stats_interval);

  if ( _proc_read && (_stats_interval != _proc_interval) ) _proc_timer.schedule_after_msec(_proc_interval);

  if ( ! _enable_full_stats ) memset(&(_small_stats[_current_small_stats]),0,sizeof(struct airtime_stats));

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

    memset(&(_small_stats[_current_small_stats]),0,sizeof(struct airtime_stats));
    _small_stats_src_tab[_current_small_stats].clear();
  }

  if ( _stats_interval > 0 ) _stats_timer.schedule_after_msec(_stats_interval);
}

void
ChannelStats::static_proc_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((ChannelStats*)f)->proc_read();
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
  EtherAddress src = EtherAddress(w->i_addr2);
  EtherAddress dst = EtherAddress(w->i_addr1);

  /* General stuff */
  _channel = BRNPacketAnno::channel_anno(p);
  if ( _device != NULL && _channel != 0 ) _device->setChannel(_channel);

  if ( !_enable_full_stats ) {
    small_stats->last = p->timestamp_anno();
  }

  /* Handle TXFeedback */
  if ( ceh->flags & WIFI_EXTRA_TX ) {

    for ( int i = 0; i < (int) ceh->retries; i++ ) {
      int t0,t1,t2,t3;
      t0 = ceh->max_tries + 1;
      t1 = t0 + ceh->max_tries1;
      t2 = t1 + ceh->max_tries2;
      t3 = t2 + ceh->max_tries3;

      uint16_t rate = 0;
      if ( i < t0 ) rate = ceh->rate;
      else if ( i < t1 ) rate = ceh->rate1;
      else if ( i < t2 ) rate = ceh->rate2;
      else if ( i < t3 ) rate = ceh->rate3;

      if (_enable_full_stats) {

        PacketInfo *new_pi = new PacketInfo();
        new_pi->_src = src;
        new_pi->_dst = dst;
        new_pi->_rx_time = p->timestamp_anno();
        new_pi->_length = p->length()+4;
        new_pi->_foreign = false;
        new_pi->_channel = _channel;
        new_pi->_rx = false;
        new_pi->_rate = rate;

        if (rate != 0) {
          new_pi->_duration = calc_transmit_time(rate, p->length());
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
          small_stats->duration_tx += calc_transmit_time(rate, p->length()); // update duration counter
          small_stats->txpackets++;
        } else {
          small_stats->zero_rate_packets++;
        }
      }
    }
  } else {   /* Handle Rx Packets */

    PacketInfo *new_pi;

    if ( _enable_full_stats ) {
      new_pi = new PacketInfo();
      new_pi->_rx_time = p->timestamp_anno();
      new_pi->_length = p->length() + 4;
      new_pi->_foreign = true;
      new_pi->_channel = _channel;
      new_pi->_rx = true;
      new_pi->_rate = ceh->rate;
    }

    if ( ceh->rate != 0 ) {

      uint32_t duration = calc_transmit_time(ceh->rate, p->length() + 4 /*crc*/);
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
      } else { // fast mode
        // update duration counter
        small_stats->duration_rx += duration;
        small_stats->rxpackets++;

        int silence = (signed char)ceh->silence;
        small_stats->avg_noise += silence;
        small_stats->std_noise += (silence * silence);
        int rssi = (signed char)ceh->rssi;

        small_stats->avg_rssi += rssi;
        small_stats->std_rssi += (rssi * rssi);

        switch (state) {
          case STATE_OK:
                small_stats->duration_noerr_rx += duration;
                small_stats->noerr_packets++;
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
          SrcInfo *src_info;
          if ( (src_info = _small_stats_src_tab[_current_small_stats].findp(src)) == NULL )
            _small_stats_src_tab[_current_small_stats].insert(src, SrcInfo(rssi));
          else
            src_info->add_rssi(rssi);
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
ChannelStats::addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx) {
  PacketInfoHW *new_pi = new PacketInfoHW();
  new_pi->_time = *time;

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
  String raw_info = file_string(_proc_file);
  Vector<String> args;

  cp_spacevec(raw_info, args);

  if ( args.size() > 6 ) {
    Timestamp now = Timestamp::now();

    int busy, rx, tx;
    cp_integer(args[1],&busy);
    cp_integer(args[3],&rx);
    cp_integer(args[5],&tx);

    if ( _enable_full_stats ) {
      addHWStat(&now, busy, rx, tx);
    } else {
      _small_stats[_current_small_stats].last_hw = now;
      _small_stats[_current_small_stats].hw_busy += busy;
      _small_stats[_current_small_stats].hw_rx += rx;
      _small_stats[_current_small_stats].hw_tx += tx;
      _small_stats[_current_small_stats].hw_count++;
    }
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
ChannelStats::calc_stats(struct airtime_stats *cstats, SrcInfoTable *src_tab)
{
  Timestamp now = Timestamp::now();
  Timestamp diff;
  int i = 0;

  HashMap<EtherAddress, EtherAddress> sources;

  diff = now - cstats->last_update;

  if ( diff.usecval() <= _min_update_time * 1000 ) return;

  if ( src_tab ) src_tab->clear();

  memset(cstats, 0, sizeof(struct airtime_stats));

  cstats->stats_id = _stats_id++;
  cstats->duration = _stats_interval;
  cstats->last_update = now;

  int diff_time = 10 * _stats_interval;

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

        if ( sources.findp(pi->_src) == NULL ) sources.insert(pi->_src,pi->_src);

        if ( src_tab != NULL ) {
          SrcInfo *src_i;
          if ( (src_i = src_tab->findp(pi->_src)) == NULL ) {
            src_tab->insert(pi->_src, SrcInfo((uint32_t)pi->_rssi));
          } else {
            src_i->add_rssi((uint32_t)pi->_rssi);
          }
        }
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
  _packet_list_hw.clear();
}

/**************************************************************************************************************/
/****************************** CALC STATS FINAL (use for small and full stats) *******************************/
/**************************************************************************************************************/

int32_t isqrt32(int32_t n) {
  int32_t x,x1;

  if ( n == 0 ) return 0;

  x1 = n;
  do {
    x = x1;
    x1 = (x + n/x) >> 1;
  } while ((( (x - x1) > 1 ) || ( (x - x1)  < -1 )) && ( x1 != 0 ));

  return x1;
}

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
    BRN_ERROR("Overflow");
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

  } else {
    small_stats->avg_noise = -100; // default value
    small_stats->std_noise = 0;
    small_stats->avg_rssi = 0;
    small_stats->std_rssi = 0;
  }

  if ( small_stats->hw_count > 0 ) {
    small_stats->hw_busy /= small_stats->hw_count;
    small_stats->hw_rx /= small_stats->hw_count;
    small_stats->hw_tx /= small_stats->hw_count;
  }

  if ( src_tab ) {
    small_stats->no_sources = src_tab->size();

    for (SrcInfoTableIter iter = src_tab->begin(); iter.live(); iter++) {
      EtherAddress ea = iter.key();
      SrcInfo *src = src_tab->findp(ea);
      src->calc_values();
    }
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

  struct airtime_stats *stats;
  SrcInfoTable         *src_tab;

  if ( _enable_full_stats ) {
    if (_neighbour_stats)
      calc_stats(&_full_stats, &_full_stats_srcinfo_tab);
    else
      calc_stats(&_full_stats, NULL);

    stats = &_full_stats;
    src_tab = &_full_stats_srcinfo_tab;
  } else {
    stats = &(_small_stats[(_current_small_stats + SMALL_STATS_SIZE - 1)%SMALL_STATS_SIZE]);
    src_tab = &(_small_stats_src_tab[(_current_small_stats + SMALL_STATS_SIZE - 1)%SMALL_STATS_SIZE]);
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
      sa << "\" rx_bcast_pkt=\"" << stats->rx_bcast_packets;
      sa << "\" tx_unicast_pkt=\"" << stats->tx_ucast_packets << "\" tx_retry_pkt=\"" << stats->tx_retry_packets;
      sa << "\" tx_bcast_pkt=\"" << stats->tx_bcast_packets;
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

      sa << "\t<phy hwbusy=\"" << stats->hw_busy << "\" hwrx=\"" << stats->hw_rx << "\" hwtx=\"" << stats->hw_tx << "\" ";
      sa << "last_hw_stat_time=\"" << stats->last_hw.unparse() << "\" hw_stats_count=\"" << stats->hw_count << "\" ";
      sa << "avg_noise=\"" << stats->avg_noise << "\" std_noise=\"" << stats->std_noise;
      sa << "\" avg_rssi=\"" << stats->avg_rssi << "\" std_rssi=\"" << stats->std_rssi;
      sa << "\" channel=\"";
      if ( _device ) sa << (int)(_device->getChannel());
      else           sa << _channel;
      sa << "\" />";

      sa << "\n\t<neighbourstats>\n";
      for (SrcInfoTableIter iter = src_tab->begin(); iter.live(); iter++) {
        SrcInfo src = iter.value();
        EtherAddress ea = iter.key();
        sa << "\t\t<nb addr=\"" << ea.unparse() << "\" rssi=\"" << src._avg_rssi << "\" std_rssi=\"";
        sa  << src._std_rssi << "\" min_rssi=\"" << src._min_rssi << "\" max_rssi=\"" << src._max_rssi;
        sa << "\" pkt_cnt=\"" << src._pkt_count << "\">\n";
        sa << "\t\t\t<rssi_hist size=\"" << src._rssi_hist_index << "\" max_size=\"" << src._rssi_hist_size;
        sa << "\" overflow=\"" << src._rssi_hist_overflow << "\" values=\"";
        for ( uint32_t rssi_i = 0; rssi_i < src._rssi_hist_index; rssi_i++) {
          if ( rssi_i > 0 ) sa << ",";
          sa << (uint32_t)(src._rssi_hist[rssi_i]);
        }
        sa << "\" />\n\t\t</nb>\n";
      }
      sa << "\t</neighbourstats>\n</channelstats>\n";

  }
  return sa.take_string();
}

static String 
ChannelStats_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  ChannelStats *td = (ChannelStats *)e;
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
  ChannelStats *f = (ChannelStats *)e;
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
