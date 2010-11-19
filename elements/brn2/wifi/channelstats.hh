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

#define STATE_UNKNOWN  0
#define STATE_OK       1
#define STATE_CRC      2
#define STATE_PHY      3

#define CS_DEFAULT_STATS_DURATION       100
#define CS_DEFAULT_SAVE_DURATION          0
#define CS_DEFAULT_PROCINTERVAL           0
#define CS_DEFAULT_PROCREAD           false
#define CS_DEFAULT_MIN_UPDATE_TIME       20
#define CS_DEFAULT_RSSI_PER_NEIGHBOUR  true
#define CS_DEFAULT_STATS_TIMER        false


#define CHANNEL_UTILITY_INVALID 255
#define RSSI_LIMIT 100

CLICK_DECLS

/*
=c
ChannelStats()



=d


=a 

*/

struct airtime_stats {
  Timestamp last_update;

  Timestamp last;
  Timestamp last_hw;
  int packets;
  int busy;
  int rx;
  int tx;
  bool hw_available;
  int hw_busy;
  int hw_rx;
  int hw_tx;
  int avg_noise;
  int avg_rssi;
  int no_sources;

  int crc_rx;
  int crc_count;
  int phy_rx;
  int phy_count;
};


class ChannelStats : public Element {

  public:
    class PacketInfo {
     public:
      Timestamp _rx_time;
      unsigned int _duration;
      uint16_t _rate;
      uint16_t _length;
      bool _foreign;
      int _channel;
      bool _rx;
      int _noise;
      int _rssi;
      EtherAddress _src;
      uint8_t _state;
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
      uint32_t _pkt_count;

      SrcInfo() {
         _rssi = 0;
         _pkt_count = 0;
      }

      SrcInfo(uint32_t rssi, uint32_t pkt_count) {
        if ( rssi > RSSI_LIMIT ) _rssi = 0; 
        else _rssi = rssi;
        _pkt_count = pkt_count;
      }

     void add_rssi(uint32_t rssi) {
       if ( rssi <= RSSI_LIMIT ) _rssi += rssi;
       _pkt_count++;
     }

     uint32_t avg_rssi() {
       return (_rssi/_pkt_count);
     }
    };

    typedef Vector<PacketInfo*> PacketList;
    typedef PacketList::const_iterator PacketListIter;

    typedef Vector<PacketInfoHW*> PacketListHW;
    typedef PacketListHW::const_iterator PacketListHWIter;

    typedef HashMap<EtherAddress, SrcInfo> RSSITable;
    typedef RSSITable::const_iterator RSSITableIter;

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
    void reset();

    String stats_handler(int mode);

    void calc_stats(struct airtime_stats *stats, RSSITable *rssi_tab);
    void get_stats(struct airtime_stats *cstats, int /*time*/);


    void addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx);

    bool _debug;
    int32_t _save_duration;  //maximum age of pakets in the queue in seconds
    int32_t _stats_duration; //maximum age of pakets, which are considered in the calculation (default)

  private:

    void readProcHandler();

    PacketList _packet_list;
    PacketListHW _packet_list_hw;

    uint32_t hw_busy;
    uint32_t hw_rx;
    uint32_t hw_tx;

    void clear_old();
    void clear_old_hw();

    struct airtime_stats stats;
    RSSITable rssi_tab;

    bool _rssi_per_neighbour;

    String _proc_file;
    bool _proc_read;

    uint32_t _min_update_time;

    bool _stats_timer_enable;
    int _stats_interval;
    Timer _stats_timer;

    uint32_t _stats_id;

};

CLICK_ENDDECLS
#endif
