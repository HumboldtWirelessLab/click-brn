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

#ifndef SEISMODETECTIONLONGSHORTAVG_ELEMENT_HH
#define SEISMODETECTIONLONGSHORTAVG_ELEMENT_HH
#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/brn2.h"

#include "seismo_reporting.hh"

#define SEISMO_REPORT_DEFAULT_LONG_INTERVAL    1000
#define SEISMO_REPORT_DEFAULT_SHORT_INTERVAL   100

//Hopefully this threshold are big limits, so that no alarm will be detected
#define DEFAULT_DIFF_THRESHOLD 2147483647
#define DEFAULT_RATIO_THRESHOLD 2147483647
#define DEFAULT_NORMALIZE 10000

//TODO: rename to LTA_STA

CLICK_DECLS

class SlidingWindow {
 public:

  int32_t _inserts;
  bool    _calc_stats;
  bool    _calc_min_max;

  int32_t _history_size;
  int32_t _history_index;
  bool    _history_complete;
  int32_t _history_no_values;

  int32_t _window_size;
  int32_t _window_index;
  int32_t _window_start;
  bool    _window_complete;
  int32_t _window_no_values;

  int32_t *_raw;
  int32_t *_fixed_value;

  int64_t _history_cum_raw_vals;
  int32_t _history_current_mean;
  int64_t _history_cum_vals, _history_cum_sq_vals, _history_cum_abs_vals;
  int32_t _history_stdev, _history_min, _history_max, _history_avg, _history_sq_avg;
  int32_t _history_abs_avg;

  int64_t _window_cum_vals, _window_cum_sq_vals, _window_cum_abs_vals;
  int32_t _window_stdev, _window_min, _window_max, _window_avg, _window_sq_avg;
  int32_t _window_abs_avg;

  uint32_t _debug;
  uint32_t _samplerate;

 public:

  SlidingWindow(uint32_t window_size, int32_t history_size)  {
    _history_size = history_size;
    _history_index = -1;  //point to the last val. at the beginning it is -1
    _history_complete = false;
    _history_no_values = 0;

    _window_size = window_size;
    _window_start = 0;
    _window_index = -1;
    _window_complete = false;
    _window_no_values = 0;

    _raw = new int32_t[_history_size];
    _fixed_value = new int32_t[_history_size];

    _calc_stats = false;
    _inserts = 0;

    _history_cum_raw_vals = _history_current_mean = 0;
    _history_cum_vals = _history_cum_sq_vals = _history_cum_abs_vals = 0;
    _history_stdev = _history_min = _history_max = _history_avg = _history_sq_avg = 0;
    _history_abs_avg = 0;

    _window_cum_vals = _window_cum_sq_vals = _window_cum_abs_vals = 0;
    _window_stdev = _window_min = _window_max = _window_avg = _window_sq_avg = 0;
    _window_abs_avg = 0;

    _debug = BrnLogger::DEFAULT;

  }

  SlidingWindow() {
    SlidingWindow(SEISMO_REPORT_DEFAULT_SHORT_INTERVAL,SEISMO_REPORT_DEFAULT_LONG_INTERVAL);
  }

  inline int32_t add_data(int32_t value) {
    //Set flags to mark that new data is available and stats need to be recalculate
    _calc_stats = false;
    _inserts++;

    //Set pointer to next index (position for new data
    _history_index = (_history_index + 1)%_history_size;
    _window_index = (_window_index + 1)%_history_size;

    //add raw value to get avg of raw values
    _history_cum_raw_vals += (int64_t)value;

    //remove old value from history data
    if ( _history_complete ) {
      _history_cum_sq_vals -= (int64_t)_fixed_value[_history_index] *
                              (int64_t)_fixed_value[_history_index];
      _history_cum_vals -= (int64_t)_fixed_value[_history_index];
      _history_cum_raw_vals -= (int64_t)_raw[_history_index];

      if (_fixed_value[_history_index] < 0)
        _history_cum_abs_vals+=(int64_t)_fixed_value[_history_index];
      else
        _history_cum_abs_vals-=(int64_t)_fixed_value[_history_index];

      _history_no_values--;
    }

    //get new avg for raw values to correct values for history and slidung window
    _history_current_mean = (int32_t)(_history_cum_raw_vals / (int64_t)history_size());

    if ( _window_complete ) {
      _window_cum_sq_vals -= (int64_t)_fixed_value[_window_start] *
                             (int64_t)_fixed_value[_window_start];
      _window_cum_vals -= (int64_t)_fixed_value[_window_start];
      _window_start = (_window_start + 1)%_history_size;

      if (_fixed_value[_window_start] < 0) _window_cum_abs_vals+=(int64_t)_fixed_value[_window_start];
      else _window_cum_abs_vals-=(int64_t)_fixed_value[_window_start];

      _window_no_values--;
    }

    if ( (_window_index+1) == _window_size ) _window_complete = true;
    if ( (_history_index+1) == _history_size ) _history_complete = true;

    _raw[_history_index] = value;

    int64_t corr_value = (int64_t)value - (int64_t)_history_current_mean;
    
    //click_chatter("Value: %d Corr: %lli Mean: %lli", value, (int64_t)corr_value, (int64_t)_history_current_mean);
    
    _fixed_value[_history_index] = corr_value;
    _history_cum_vals += corr_value;
    _window_cum_vals += corr_value;

    if (corr_value < 0) {
      _history_cum_abs_vals -= corr_value;
      _window_cum_abs_vals -= corr_value;
    } else {
      _history_cum_abs_vals += corr_value;
      _window_cum_abs_vals += corr_value;
    }

    int64_t corr_value_sqr = (int64_t)corr_value*(int64_t)corr_value;
    _history_cum_sq_vals += corr_value_sqr;
    _window_cum_sq_vals += corr_value_sqr;

    _history_no_values++;
    _window_no_values++;

    NON_BRN_INFO("--------------------------------------------------------------");
    NON_BRN_INFO("Insert: %d Cum: %d",_inserts,_history_cum_raw_vals);
    NON_BRN_INFO("History: Index: %d NoE: %d Complete: %d Cum: %lli CumSq: %lli",
                            _history_index,_history_no_values,_history_complete?1:0,
                            _history_cum_vals,_history_cum_sq_vals);
    NON_BRN_INFO("Window: Start: %d Index: %d NoE: %d Complete: %d Cum: %lli CumSq: %lli",
                       _window_start, _window_index,_window_no_values,_window_complete?1:0,
                       _window_cum_vals,_window_cum_sq_vals);

    return _history_complete ? 0 : 1;
  }

  void calc_stats(bool calc_min_max) {
    if ( _calc_stats && ( _calc_min_max || (!calc_min_max)) ) return;
    if ( _history_index == -1 ) return;

    _calc_stats = true;
    _calc_min_max = calc_min_max;

    int32_t hist_size = history_size();
    int32_t win_size = window_size();

    if ( hist_size > 0 ) {

      int64_t history_stdev;

      history_stdev = (int64_t)((int64_t)_history_cum_vals / (int64_t)hist_size);
      history_stdev *= history_stdev;
      history_stdev = (_history_cum_sq_vals/(int64_t)hist_size) - history_stdev;

      if ( history_stdev < 0 ) {
        NON_BRN_ERROR("History error: isqrt from neg");
      } else {
        _history_stdev = isqrt64(history_stdev);
      }

      _history_avg = _history_cum_vals/(int64_t)hist_size;
      _history_abs_avg = _history_cum_abs_vals/(int64_t)hist_size;
      _history_sq_avg = _history_cum_sq_vals/(int64_t)hist_size;

      int64_t window_stdev;

      window_stdev = (int64_t)((int64_t)_window_cum_vals / (int64_t)win_size);
      window_stdev *= window_stdev;
      window_stdev = (_window_cum_sq_vals/(int64_t)win_size) - window_stdev;

      if ( window_stdev < 0 ) {
        NON_BRN_ERROR("Window error: isqrt from neg");
      } else {
        _window_stdev = isqrt64(window_stdev);
      }

      _window_avg = _window_cum_vals/(int64_t)win_size;
      _window_abs_avg = _window_cum_abs_vals/(int64_t)win_size;
      _window_sq_avg = _window_cum_sq_vals/(int64_t)win_size;

    }

    if ( calc_min_max ) {
      _history_min = _window_min = 2147483647;
      _history_max = _window_max = -2147483647;

      for ( int i = 0; i < hist_size; i++ ) {
        if ( _fixed_value[i] < _history_min ) _history_min = _fixed_value[i];
        else if ( _fixed_value[i] > _history_max ) _history_max = _fixed_value[i];
      }

      int32_t index = _window_start;
      for ( int32_t i = 0; i < win_size; i++ ) {
        if ( _fixed_value[index] < _window_min ) _window_min = _fixed_value[index];
        else if ( _fixed_value[index] > _window_max ) _window_max = _fixed_value[index];
        index = (index + 1)%_history_size;
      }
    }
  }

  inline int32_t history_size() {
    return _history_complete ? _history_size : (_history_index + 1);
  }

  inline int32_t window_size() {
    return _window_complete ? _window_size : (_window_index + 1);
  }

};

#define ALARM_MODE_START true
#define ALARM_MODE_END   false

class SeismoAlarmLTASTAInfo {
  public:

    int32_t _stdev_long;
    int32_t _stdev_short;
    int32_t _avg_long;
    int32_t _avg_short;
    int32_t _sq_avg_long;
    int32_t _sq_avg_short;

    uint32_t _insert; //number of data (start of alarm)
    
    bool _mode;

    SeismoAlarmLTASTAInfo(int32_t stdev_long, int32_t stdev_short, int32_t avg_long,
                         int32_t avg_short, int32_t sq_avg_long, int32_t sq_avg_short, uint32_t insert) {
      _mode = ALARM_MODE_START;
      _stdev_long = stdev_long;
      _stdev_short = stdev_short;
      _avg_long = avg_long;
      _avg_short = avg_short;
      _sq_avg_long = sq_avg_long;
      _sq_avg_short = sq_avg_short;
      _insert = insert;
    }

    void update(int32_t stdev_long, int32_t stdev_short, int32_t avg_long,
                int32_t avg_short, int32_t sq_avg_long, int32_t sq_avg_short, uint32_t insert) {
      _mode = ALARM_MODE_START;
      _stdev_long = stdev_long;
      _stdev_short = stdev_short;
      _avg_long = avg_long;
      _avg_short = avg_short;
      _sq_avg_long = sq_avg_long;
      _sq_avg_short = sq_avg_short;
      _insert = insert;
    }

    void end_alarm() {
      _mode = ALARM_MODE_END;
    }
};

class SeismoDetectionLongShortAvg : public BRNElement, public SeismoDetectionAlgorithm {

  public:

  SeismoDetectionLongShortAvg();
  ~SeismoDetectionLongShortAvg();

  const char *class_name() const  { return "SeismoDetectionLongShortAvg"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  void *cast(const char *);

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();

  void update(SrcInfo *si, uint32_t next_new_block);
  SeismoAlarmList *get_alarm() { return NULL; };

  uint32_t _long_avg_count;
  uint32_t _short_avg_count;

  uint32_t _dthreshold;
  uint32_t _rthreshold;
  uint32_t _normalize;

  uint32_t _max_alarm_count;

  SlidingWindow swin;
  SeismoAlarmList sal;
  
  bool _print_alarm;
  bool _init_swin;

};

CLICK_ENDDECLS
#endif
