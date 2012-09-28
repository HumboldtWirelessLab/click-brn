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

#ifndef SEISMOREPORTING_ELEMENT_HH
#define SEISMOREPORTING_ELEMENT_HH
#include <click/element.hh>
#include <click/timer.hh>

#include "elements/brn2/brnelement.hh"

#include "seismo.hh"

#define SEISMO_REPORT_DEFAULT_INTERVAL 500
#define SEISMO_REPORT_LONG_INTERVAL    1000
#define SEISMO_REPORT_SHORT_INTERVAL   100

#define SEISMO_REPORT_MAX_ALARM_COUNT  100

CLICK_DECLS

class SlidingWindow {
 public:

  int32_t _window_size;
  int32_t _history_size;
  int32_t _history_index;
  bool    _history_complete;

  int32_t *_avg;
  int32_t *_raw_avg;
  int32_t *_min;
  int32_t *_max;
  int64_t *_stdev;

 private:

  int32_t _no_data, _cur_min, _cur_max, _cur_stdev, _cum_vals, _cum_raw_vals, _current_mean;
  int64_t _cum_sq_vals;

 public:
  SlidingWindow(uint32_t window_size, int32_t history_size)  {
    _window_size = window_size;
    _history_size = history_size;
    _history_index = 0;
    _history_complete = false;
    _avg = new int32_t[_history_size];
    _raw_avg = new int32_t[_history_size];
    _min = new int32_t[_history_size];
    _max = new int32_t[_history_size];
    _stdev = new int64_t[_history_size];

    _current_mean = 0;

    reset();
  }

  void reset() {
    _no_data = _cum_vals = _cur_min = _cur_max = _cur_stdev = _cum_sq_vals = _cum_raw_vals = 0;
  }

  inline int32_t add_data(int32_t value) {
    int32_t corr_value = value - _current_mean;

    if ( _no_data == 0 ) {
      _cum_raw_vals = value;
      _cum_vals = _cur_min = _cur_max = _cur_stdev = corr_value;
      _cum_sq_vals = (int64_t)corr_value * (int64_t)corr_value;
      _no_data++;
    } else {
      _cum_raw_vals += value;
      _cum_vals += corr_value;
      _cum_sq_vals += (int64_t)corr_value * (int64_t)corr_value;

      if ( corr_value < _cur_min ) {
        _cur_min = corr_value;
      } else {
        if ( corr_value > _cur_max ) {
          _cur_max = corr_value;
        }
      }

      _no_data++;

      if ( _no_data == _window_size ) {
        calc_stats();
        reset();

        if ( _history_complete ) return 1;
      }
    }

    return 0;
  }

  int64_t isqrt(int64_t n) {
    int64_t x,x1;

    if ( n == 0 ) return 0;

    x1 = n;
    do {
      x = x1;
      x1 = (x + n/x) >> 1;
    } while ((( (x - x1) > 1 ) || ( (x - x1)  < -1 )) && ( x1 != 0 ));

    return x1;
  }

  void calc_stats() {
    _current_mean = _raw_avg[_history_index] = _cum_raw_vals / _no_data;
    _avg[_history_index] = _cum_vals / _no_data;
    _min[_history_index] = _cur_min;
    _max[_history_index] = _cur_max;

    _stdev[_history_index] = (int64_t)_avg[_history_index] * (int64_t)_avg[_history_index];
    //click_chatter("%d %d %d %d %d",_stdev[_history_index],_avg[_history_index],_cum_sq_vals,_no_data, _cum_vals);
    _stdev[_history_index] = (_cum_sq_vals/(int64_t)_no_data) - _stdev[_history_index];

    if ( _history_complete ) {
      //click_chatter("Calc %d",_stdev[_history_index]);
      if ( _stdev[_history_index] < 0 ) {
        _stdev[_history_index] = 0;
        //click_chatter("Neg cum sq");
      } else {
        _stdev[_history_index] = isqrt(_stdev[_history_index]);
      }

      _current_mean = 0;
      for ( uint16_t i = 0; i < _history_size; i++ ) _current_mean += _raw_avg[_history_index];
      _current_mean /= _history_size;

    } else {
      _stdev[_history_index] = 0;
    }

    _history_index = (_history_index + 1) % _history_size;
    if ( _history_index == 0 ) _history_complete = true;

  }

  int32_t size() {
    if ( _history_complete ) return _history_size;
    return _history_index - 1;
  }

  inline int32_t get_history_index(int32_t i) {
    return ( i + _history_index + _history_size) % _history_size;
  }

};

typedef Vector<SlidingWindow> SlidingWindowList;
typedef SlidingWindowList::const_iterator SlidingWindowListIter;

#define ALARM_MODE_START 0
#define ALARM_MODE_END   1

class SeismoAlarm {
  public:

    int32_t _stdev_before;
    int32_t _stdev_during;
    int32_t _stdev_after;
    uint8_t _mode;

    Timestamp _start;
    Timestamp _end;

    SeismoAlarm(int32_t before, int32_t during) {
      _start = Timestamp::now();
      _stdev_before = before;
      _stdev_during = during;
      _mode = ALARM_MODE_START;
    }

    void end_alarm(int32_t end) {
      _end = Timestamp::now();
      _stdev_after = end;
      _mode = ALARM_MODE_END;
    }
};

typedef Vector<SeismoAlarm> SeismoAlarmList;
typedef SeismoAlarmList::const_iterator SeismoAlarmListIter;

class SeismoAlarmAlgorithm {
 public:
  virtual void update(SlidingWindowList *) = 0;
  virtual SeismoAlarmList *get_alarm() = 0;
};

class SeismoReporting : public BRNElement {

  public:

  SeismoReporting();
  ~SeismoReporting();

  const char *class_name() const  { return "SeismoReporting"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; } //0: local 1: remote

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void run_timer(Timer *);

  void add_handlers();

  void seismo_evaluation();

  String print_stats();
  String print_alarm();

  Timer _reporting_timer;

  Seismo *_seismo;

  uint32_t _next_block_id;
  uint32_t _index_in_block;

  uint32_t _interval, _long_avg_count, _short_avg_count;

  SlidingWindowList swl;
  SeismoAlarmList sal;

  int32_t _max_alarm_count;
};

CLICK_ENDDECLS
#endif
