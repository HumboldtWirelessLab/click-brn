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

#ifndef CLICK_CHANNELSTATS_HH
#define CLICK_CHANNELSTATS_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"
#include "elements/brn2/routing/identity/brn2_device.hh"

#define STATE_UNKNOWN  0
#define STATE_OK       1
#define STATE_CRC      2
#define STATE_PHY      3
#define STATE_ERROR    4


#define CS_DEFAULT_STATS_DURATION      1000
#define CS_DEFAULT_SAVE_DURATION          0
#define CS_DEFAULT_MIN_UPDATE_TIME      200
#define CS_DEFAULT_RSSI_PER_NEIGHBOUR  true

#define CS_DEFAULT_RSSI_HIST_SIZE        50

#define SMALL_STATS_SIZE          2
#define CHANNEL_UTILITY_INVALID 255

CLICK_DECLS

/*
=c
ChannelStats()

=d

=a

*/

struct airtime_stats {
  uint32_t stats_id;

  uint32_t duration;

  Timestamp last_update;

  Timestamp last;
  Timestamp last_hw;

  uint32_t rxpackets;

  uint32_t noerr_packets;
  uint32_t crc_packets;
  uint32_t phy_packets;
  uint32_t unknown_err_packets;

  uint32_t zero_rate_packets;

  uint32_t rx_ucast_packets;
  uint32_t rx_bcast_packets;
  uint32_t rx_retry_packets;

  uint32_t txpackets;

  uint32_t tx_ucast_packets;
  uint32_t tx_bcast_packets;
  uint32_t tx_retry_packets;

  /* fraction of time */
  uint32_t frac_mac_busy;
  uint32_t frac_mac_rx;
  uint32_t frac_mac_tx;

  uint32_t frac_mac_noerr_rx;
  uint32_t frac_mac_crc_rx;
  uint32_t frac_mac_phy_rx;
  uint32_t frac_mac_unknown_err_rx;

  /* duration of packet-rec in ms */
  uint32_t duration_busy;
  uint32_t duration_rx;
  uint32_t duration_tx;
  uint32_t duration_noerr_rx;
  uint32_t duration_crc_rx;
  uint32_t duration_phy_rx;
  uint32_t duration_unknown_err_rx;

  /*Hardware stats available? fraction of time (hw) */
  bool     hw_available;
  uint32_t hw_busy;
  uint32_t hw_rx;
  uint32_t hw_tx;
  uint32_t hw_count;

  int32_t avg_noise;
  int32_t std_noise;
  int32_t avg_rssi;
  int32_t std_rssi;
  int32_t no_sources;

};


class ChannelStats : public BRNElement {

  public:
    class PacketInfo {
     public:
      Timestamp _rx_time;
      uint32_t _duration;
      uint16_t _rate;
      uint16_t _length;
      bool _foreign;
      int32_t _channel;
      bool _rx;
      int32_t _noise;
      int32_t _rssi;
      EtherAddress _src;
      EtherAddress _dst;
      uint8_t _state;
      bool _retry;
      bool _unicast;

      uint8_t _mode;
      uint8_t _mcs_index;
      uint8_t _mcs_flags;
    };

    class PacketInfoHW {
      public:
        Timestamp _time;
        uint32_t _busy;
        uint32_t _rx;
        uint32_t _tx;
    };

    class SrcInfo {
     public:
      uint32_t _rssi;
      uint32_t _sum_sq_rssi;
      uint32_t _pkt_count;

      uint32_t _avg_rssi;
      uint32_t _std_rssi;

      uint32_t _min_rssi;
      uint32_t _max_rssi;

      bool _calc_finished;

      uint8_t _mode;
      uint8_t _mcs_index;
      uint8_t _mcs_flags;

      uint8_t _rssi_hist[CS_DEFAULT_RSSI_HIST_SIZE];
      uint32_t _rssi_hist_index;
      uint32_t _rssi_hist_size;
      bool _rssi_hist_overflow;

      SrcInfo(): _rssi(0), _sum_sq_rssi(0), _pkt_count(0),
                 _min_rssi(1000), _max_rssi(0), _calc_finished(false),
                 _rssi_hist_index(0), _rssi_hist_size(CS_DEFAULT_RSSI_HIST_SIZE), _rssi_hist_overflow(false)
      {  //TODO: better start value for min_rssi (replace 1000)
      }

      SrcInfo(uint32_t rssi): _rssi(0), _sum_sq_rssi(0), _pkt_count(0),
                              _min_rssi(1000), _max_rssi(0), _calc_finished(false), _rssi_hist_index(0),
                              _rssi_hist_size(CS_DEFAULT_RSSI_HIST_SIZE), _rssi_hist_overflow(false)
      {
        if ( rssi != 0 ) {
          _rssi = rssi;
          _sum_sq_rssi = rssi * rssi;
        }

        _min_rssi = _max_rssi = rssi;
        _rssi_hist[_rssi_hist_index++] = rssi;
        _pkt_count = 1;
      }

     void add_rssi(uint32_t rssi) {
       if ( rssi > 0 ) {
         _rssi += rssi;
         _sum_sq_rssi += rssi * rssi;
         if ( rssi > _max_rssi ) _max_rssi = rssi;
         else if ( rssi < _min_rssi ) _min_rssi = rssi;
       } else {
         _min_rssi = 0;
       }

       if ( _rssi_hist_index < _rssi_hist_size ) {
         _rssi_hist[_rssi_hist_index++] = rssi;
       } else {
         _rssi_hist_overflow = true;
         _rssi_hist[click_random() % _rssi_hist_size] = rssi;
       }

       _pkt_count++;

     }

     uint32_t avg_rssi() {
       return (_rssi/_pkt_count);
     }

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

     uint32_t std_rssi() {
       int32_t q = _rssi/_pkt_count;
       return isqrt32((_sum_sq_rssi/_pkt_count)-(q*q));
     }

     void calc_values() {
       if ( _calc_finished ) return;
       _avg_rssi = avg_rssi();
       _std_rssi = std_rssi();
       _calc_finished = true;
     }
    };

    class LinkInfo {
      public:

    };

    typedef Vector<PacketInfo*> PacketList;
    typedef PacketList::const_iterator PacketListIter;

    typedef Vector<PacketInfoHW*> PacketListHW;
    typedef PacketListHW::const_iterator PacketListHWIter;

    typedef HashMap<EtherAddress, SrcInfo> SrcInfoTable;
    typedef SrcInfoTable::const_iterator SrcInfoTableIter;

  public:

    ChannelStats();
    ~ChannelStats();

    const char *class_name() const	{ return "ChannelStats"; }
    const char *processing() const  { return PUSH; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);
    int initialize(ErrorHandler *);
    void run_timer(Timer *);

    void add_handlers();

    void push(int, Packet *p);
    void reset_small_stats();
    void reset();

    String stats_handler(int mode);

    void addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx);

    void calc_stats(struct airtime_stats *stats, SrcInfoTable *src_tab);
    void get_stats(struct airtime_stats *cstats, int /*time*/);
    void calc_stats_final(struct airtime_stats *small_stats, SrcInfoTable *src_tab, int duration);

  private:

    BRN2Device *_device;

  public:
    int32_t _stats_interval; //maximum age of pakets, which are considered in the calculation (default)

  private:

    String _proc_file;
    bool _proc_read;
    int _proc_interval;

    bool _neighbour_stats;

    bool _enable_full_stats;

    int32_t _save_duration;  //maximum age of pakets in the queue in seconds

    uint32_t _min_update_time;

    uint32_t _stats_id;

    uint32_t _channel;

    PacketList _packet_list;
    PacketListHW _packet_list_hw;

    struct airtime_stats _full_stats;
    SrcInfoTable         _full_stats_srcinfo_tab;

    struct airtime_stats _small_stats[SMALL_STATS_SIZE];
    SrcInfoTable _small_stats_src_tab[SMALL_STATS_SIZE];
    uint8_t _current_small_stats;

    Timer _stats_timer;
    Timer _proc_timer;

    void readProcHandler();

    void clear_old();     //packet_stats
    void clear_old_hw();  //hw_stats

    static void static_proc_timer_hook(Timer *, void *);
    void proc_read();

  public:

    void set_channel(uint32_t channel) { _channel = channel; }                     //TODO: remove this
    uint32_t get_channel() { return _device ? _device->getChannel() : _channel; }  //TODO: remove this

};

CLICK_ENDDECLS
#endif
