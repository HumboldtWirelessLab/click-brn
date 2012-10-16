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

#ifndef SEISMO_ELEMENT_HH
#define SEISMO_ELEMENT_HH
#include <click/element.hh>
#include "elements/brn/brnelement.hh"
#include "elements/brn/brn2.h"

#include "elements/brn/services/sensor/gps/gps.hh"

CLICK_DECLS

#define SEISMO_TAG_LENGTH_LONG  0
#define SEISMO_TAG_LENGTH_SHORT 1

#define CHANNEL_INFO_BLOCK_SIZE       100
#define MAX_CHANNEL_INFO_BLOCK_COUNT   10

#define CLICK_SEISMO_VERSION 0x8080
#define CLICK_SEISMO_FLAG_SYSTIME_SET 1

CLICK_SIZE_PACKED_STRUCTURE(
struct click_seismo_data_header {,
  uint16_t version;
  uint16_t flags;
  int32_t gps_lat;
  int32_t gps_long;
  int32_t gps_alt;
  int32_t gps_hdop;

  int32_t sampling_rate;
  int32_t samples;
  int32_t channels;

  struct {
    uint64_t tv_sec;
    uint32_t tv_usec;
  } time __attribute__((packed));
});

CLICK_SIZE_PACKED_STRUCTURE(
struct click_seismo_data {,
  uint64_t time;
  int32_t timing_quality;
});

class SeismoInfoBlock {
  public:
    uint64_t _time[CHANNEL_INFO_BLOCK_SIZE];
    uint64_t _systime[CHANNEL_INFO_BLOCK_SIZE];

    int32_t _channel_values[CHANNEL_INFO_BLOCK_SIZE][4]; //raw data
    uint8_t _channels[CHANNEL_INFO_BLOCK_SIZE];

    int64_t _channel_mean[4];                            //mean data

    uint32_t _block_index;
    uint16_t _next_value_index;

    uint16_t _next_systemtime_index;

    SeismoInfoBlock(uint32_t block_index) :_next_value_index(0),_next_systemtime_index(0)
    {
      _block_index = block_index;
    }

    void reset() {
      _next_systemtime_index = 0;
      _next_value_index = 0;
    }

    inline int32_t insert(uint64_t time, uint64_t systime, uint32_t channels, int32_t *values, bool net2host = true) {
      if ( _next_value_index == CHANNEL_INFO_BLOCK_SIZE ) return -1;

      _channels[_next_value_index] = (uint8_t)channels;
      _time[_next_value_index] = time;
      _systime[_next_value_index] = systime;

      if ( net2host ) {
        for ( uint32_t i = 0; i < channels; i++ ) {
          _channel_values[_next_value_index][i] = (uint32_t)ntohl(values[i]);
        }
      } else {
        for ( uint32_t i = 0; i < channels; i++ ) {
          _channel_values[_next_value_index][i] = values[i];
        }
      }

      _next_value_index++;

      return _next_value_index;
    }

    inline void update_systemtime(uint64_t systime) {
      _systime[_next_systemtime_index++] = systime;
    }

    inline bool is_complete() { return (_next_value_index == CHANNEL_INFO_BLOCK_SIZE); }
    inline bool systime_complete() { return  (_next_systemtime_index == _next_value_index); }
    inline uint16_t missing_time_updates() { return (_next_value_index - _next_systemtime_index); }

};

typedef Vector<SeismoInfoBlock*> SeismoInfoBlockList;
typedef SeismoInfoBlockList::const_iterator SeismoInfoBlockListIter;

class SrcInfo {

  public:
    Timestamp _last_samples;

    int _gps_lat;
    int _gps_long;
    int _gps_alt;
    int _gps_hdop;

    int _sampling_rate;
    int _channels;
    uint64_t _first_time;
    uint64_t _last_update_time;
    uint32_t _sample_count; // number of packets

    SeismoInfoBlockList _seismo_infos;
    uint32_t _next_seismo_info_block_for_handler;
    uint32_t _max_seismo_info_blocks;

    SrcInfo() {
      update_gps(-1,-1,-1,-1);

      _sampling_rate = -1;
      _channels = -1;
      _sample_count = 0; // start from zero
      _first_time = 0;

      _next_seismo_info_block_for_handler = 0;
      _max_seismo_info_blocks = MAX_CHANNEL_INFO_BLOCK_COUNT;
    }

    void reset() {
      _sample_count = 0; // start from zero
      _first_time = 0;
    }

    SrcInfo(int gps_lat, int gps_long, int gps_alt, int gps_hdop, int sampling_rate, int channels) {
      _sampling_rate = sampling_rate;
      _channels = channels;

      _next_seismo_info_block_for_handler = 0;
      _max_seismo_info_blocks = MAX_CHANNEL_INFO_BLOCK_COUNT;

      update_gps(gps_lat, gps_long, gps_alt, gps_hdop);
      reset();
    }

    ~SrcInfo() {
      for ( int i = _seismo_infos.size()-1; i >= 0; i-- ) {
        delete _seismo_infos[i];
        _seismo_infos.erase(_seismo_infos.begin() + i);
      }
    }

    inline void update_gps(int gps_lat, int gps_long, int gps_alt, int gps_hdop) {
      _gps_lat = gps_lat;
      _gps_long = gps_long;
      _gps_alt = gps_alt;
      _gps_hdop = gps_hdop;
    }

    inline void update_time(uint64_t time ) {
      if ( _first_time == 0 ) _first_time = time;
      _last_update_time = time;
    }

    inline void inc_sample_count() { _sample_count++; }

    SeismoInfoBlock* new_block() {
      int block_index = 0;
      if ( _seismo_infos.size() != 0 )
        block_index = _seismo_infos[_seismo_infos.size()-1]->_block_index + 1;

      SeismoInfoBlock *nb;

      if ( _seismo_infos.size() == (int)_max_seismo_info_blocks ) {
        //TODO: check next 2 lines
        if ( _seismo_infos[0]->_block_index == _next_seismo_info_block_for_handler ) {
          _next_seismo_info_block_for_handler = _seismo_infos[1]->_block_index;
        }
        nb = _seismo_infos[0];
        _seismo_infos.erase(_seismo_infos.begin());
        nb->_block_index = block_index;
        nb->reset();
      } else {
        nb = new SeismoInfoBlock(block_index);
      }

      _seismo_infos.push_back(nb);

      return nb;
    }

    SeismoInfoBlock* get_block(uint32_t block_index) {
      for ( int i = 0; i < _seismo_infos.size(); i++ ) {
        if ( _seismo_infos[i]->_block_index == block_index ) return _seismo_infos[i];
      }

      return NULL;
    }

    SeismoInfoBlock* get_next_block(uint32_t block_index) {
      for ( int i = 0; i < _seismo_infos.size(); i++ ) {
        if ( _seismo_infos[i]->_block_index >= block_index ) return _seismo_infos[i];
      }

      return NULL;
    }

    SeismoInfoBlock* get_last_block() {
      if ( _seismo_infos.size() == 0 ) return NULL;
      return _seismo_infos[_seismo_infos.size()-1];
    }

    SeismoInfoBlock* get_next_to_last_block() {
      if ( _seismo_infos.size() < 2 ) return NULL;
      return _seismo_infos[_seismo_infos.size()-2];
    }
};

typedef HashMap<EtherAddress, SrcInfo*> NodeStats;
typedef NodeStats::const_iterator NodeStatsIter;

class Seismo : public BRNElement {

  public:

  Seismo();
  ~Seismo();

  const char *class_name() const  { return "Seismo"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "1-2/0"; } //0: local 1: remote

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void run_timer(Timer *);

  void add_handlers();

  void push(int, Packet *p);

  void data_file_read();

  GPS *_gps;
  bool _print;
  bool _record_data;

  NodeStats _node_stats_tab;
  String _last_channelstatinfo;  //includes the string of the last channel info (incl, info about all nodes)
                                 //used if no new data is available
  SrcInfo *_local_info;

  uint8_t _tag_len;

  long long _last_systemtime;

  String _data_file;
  uint32_t _data_file_interval;
  uint32_t _data_file_index;

  Timer _data_file_timer;

};

CLICK_ENDDECLS
#endif
