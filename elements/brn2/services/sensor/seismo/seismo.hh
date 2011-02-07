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
#include "elements/brn2/brnelement.hh"
//#include <click/string.hh>
#include <math.h>

#include "elements/brn2/services/sensor/gps/gps.hh"


CLICK_DECLS

#define min(x,y)      ((x)<(y) ? (x) : (y))
#define max(x,y)      ((x)>(y) ? (x) : (y))

struct click_seismo_data_header {
  int gps_lat;
  int gps_long;
  int gps_alt;
  int gps_hdop;

  int sampling_rate;
  int samples;
  int channels;
};

struct click_seismo_data {
  uint64_t time;
  int timing_quality;
};

class SrcInfo {
  public:
    int _gps_lat;
    int _gps_long;
    int _gps_alt;
    int _gps_hdop;

    int _sampling_rate;
    int _channels;
    uint64_t _last_update_time;
    uint32_t _sample_count; // number of packets
    int64_t *_chan_cum_vals; // sum of values
    int64_t *_chan_cum_sq_vals; // sum of squared values
    int* _chan_min_vals; // observed minima
    int* _chan_max_vals; // observed maxima

    SrcInfo() {
      _gps_lat = -1;
      _gps_long = -1;
      _gps_alt = -1;
      _gps_hdop = -1;

      _sampling_rate = -1;
      _channels = -1;
      _sample_count = 0; // start from zero
      _chan_cum_vals = NULL;
      _chan_cum_sq_vals = NULL;
      _chan_min_vals = NULL;
      _chan_max_vals = NULL;
    }

    SrcInfo(int gps_lat, int gps_long, int gps_alt, int gps_hdop, int sampling_rate, int channels) {
      _gps_lat = gps_lat;
      _gps_long = gps_long;
      _gps_alt = gps_alt;
      _gps_hdop = gps_hdop;

      _sampling_rate = sampling_rate;
      _channels = channels;
      _sample_count = 0; // start from zero
      _chan_cum_vals = new int64_t[channels];
      _chan_cum_sq_vals = new int64_t[channels];
      _chan_min_vals = new int[channels];
      _chan_max_vals = new int[channels];
      for (int i=0; i<channels; i++) {
	 _chan_cum_vals[i] = 0;
	 _chan_cum_sq_vals[i] = 0;
        _chan_min_vals[i] = INT_MAX;
        _chan_max_vals[i] = INT_MIN;
      }
    }

    SrcInfo(const SrcInfo& o) {
      _gps_lat = o._gps_lat;
      _gps_long = o._gps_long;
      _gps_alt = o._gps_alt;
      _gps_hdop = o._gps_hdop;
      _last_update_time = o._last_update_time;

      _sampling_rate = o._sampling_rate;
      _channels = o._channels;
      _sample_count = o._sample_count;
      _chan_cum_vals = new int64_t[o._channels];
      _chan_cum_sq_vals = new int64_t[o._channels];
      _chan_min_vals = new int[o._channels];
      _chan_max_vals = new int[o._channels];
      for (int i=0; i<o._channels; i++) {
	 _chan_cum_vals[i] = o._chan_cum_vals[i];
        _chan_cum_sq_vals[i] = o._chan_cum_sq_vals[i];
        _chan_min_vals[i] = o._chan_min_vals[i];
        _chan_max_vals[i] = o._chan_max_vals[i];
      }
    }

    ~SrcInfo() {
      if (_chan_cum_vals) delete [] _chan_cum_vals;
      _chan_cum_vals = NULL;
      if (_chan_cum_sq_vals) delete [] _chan_cum_sq_vals;
      _chan_cum_sq_vals = NULL;
      if (_chan_min_vals) delete [] _chan_min_vals;
      _chan_min_vals = NULL;
      if (_chan_max_vals) delete [] _chan_max_vals;
      _chan_max_vals = NULL;
    }

    void inc_sample_count() {
      _sample_count++;
    }

    void add_channel_val(int channel, int value) {
      // required for mean calc
      if (_chan_cum_vals != NULL) {
        _chan_cum_vals[channel] += value;
      }
      // required for std calc
      if (_chan_cum_sq_vals != NULL) {
        int64_t value64 = value;
        _chan_cum_sq_vals[channel] += (value64 * value64);
      }
      if (_chan_min_vals != NULL) {
        _chan_min_vals[channel] = min(value, _chan_min_vals[channel]);
      }
      if (_chan_max_vals != NULL) {
        _chan_max_vals[channel] = max(value, _chan_max_vals[channel]);
      }
    }

    int64_t isqrt(int64_t n) {
      int64_t x,x1;

      if ( n == 0 ) return 0;

      x1 = n;
      do {
        x = x1;
        x1 = (x + n/x) >> 1;
      } while ((( (x - x1)  > 1 ) || ( (x - x1)  < -1 )) && ( x1 != 0 ));

      return x1;
    }

    int avg_channel_info(int channel) {
      return (_chan_cum_vals[channel]/_sample_count);
    }

    int std_channel_info(int channel) {
      int64_t _sample_count64 = _sample_count;
      int64_t variance = (_chan_cum_vals[channel]/_sample_count64) * (_chan_cum_vals[channel]/_sample_count64);
      variance = (_chan_cum_sq_vals[channel]/_sample_count64) - variance;

      return isqrt(variance);
    }

    int min_channel_info(int channel) {
      return _chan_min_vals[channel];
    }

    int max_channel_info(int channel) {
      return _chan_max_vals[channel];
    }
};

typedef HashMap<EtherAddress, SrcInfo> NodeStats;
typedef NodeStats::const_iterator NodeStatsIter;

class Seismo : public BRNElement {

 public:

  Seismo();
  ~Seismo();

  const char *class_name() const  { return "Seismo"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/0"; } //0: local 1: remote

  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  void push(int, Packet *p);

  GPS *_gps;
  bool _print;
  bool _calc_stats;
  NodeStats _node_stats_tab;
  String _last_channelstatinfo;
};

CLICK_ENDDECLS
#endif
