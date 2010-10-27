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

#define STATE_UNKNOWN  0
#define STATE_OK       1
#define STATE_CRC      2
#define STATE_PHY      3

#define MIN_UPDATE_DIFF 10

CLICK_DECLS

/*
=c
ChannelStats()



=d


=a 

*/

class ChannelStats : public Element {

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
  };

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

    typedef Vector<PacketInfo*> PacketList;
    typedef PacketList::const_iterator PacketListIter;

    typedef Vector<PacketInfoHW*> PacketListHW;
    typedef PacketListHW::const_iterator PacketListHWIter;

  public:

    ChannelStats();
    ~ChannelStats();

    const char *class_name() const	{ return "ChannelStats"; }
    const char *processing() const  { return PUSH; }
    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &conf, ErrorHandler* errh);

    void add_handlers();

    void push(int, Packet *p);
    void reset();

    String stats_handler(int mode);
    void calc_stats(struct airtime_stats *stats);

    void addHWStat(Timestamp *time, uint8_t busy, uint8_t rx, uint8_t tx);
    void clear_old_hw();

    bool _debug;
    int32_t max_age;  //maximum age of pakets in the wueue in seconds

  private:
    PacketList _packet_list;
    PacketListHW _packet_list_hw;

    uint32_t hw_busy;
    uint32_t hw_rx;
    uint32_t hw_tx;

    void clear_old();

    struct airtime_stats stats;
};

CLICK_ENDDECLS
#endif
