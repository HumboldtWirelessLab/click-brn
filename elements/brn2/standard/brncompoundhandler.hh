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

#ifndef BRNCOMPOUNDHANDLER_HH_
#define BRNCOMPOUNDHANDLER_HH_

#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

#define UPDATEMODE_SEND_ALL  0
#define UPDATEMODE_SEND_INFO 1
#define UPDATEMODE_SEND_DIFF 2

#define RECORDMODE_LAST_ONLY  0
#define RECORDMODE_LAST_SAMP  1

class BrnCompoundHandler : public BRNElement
{
 public:

  class HandlerRecord {

   public:
    int _rec_max;
    int _rec_index;
    int _interval;

    Timestamp *_rec_times;
    String *_rec;

    HandlerRecord(): _rec_times(NULL), _rec(NULL) {
    }

    HandlerRecord(int max_records, int interval): _rec_times(NULL), _rec(NULL) {
      _rec_max = max_records;
      _rec_index = 0;
      _interval = interval;
      _rec = new String[_rec_max];
      _rec_times = new Timestamp[_rec_max];
   }

    ~HandlerRecord() {
      if ( _rec != NULL ) delete[] _rec;
      if ( _rec_times != NULL ) delete[] _rec_times;

      _rec = NULL;
      _rec_times = NULL;
    }

    void insert(const Timestamp& now, const String& value) {
      int index = _rec_index % _rec_max;
      _rec[index] = value;
      _rec_times[index] = now;
      _rec_index++;
    }

    bool overflow() {
      return _rec_index > _rec_max;
    }

    void clear() {
      _rec_index = 0;
    }

    int count_records() {
      return (_rec_index>=_rec_max)?_rec_max:_rec_index;
    }

    String *get_record_i(int i) {
      return &(_rec[(_rec_index>=_rec_max)?((i+_rec_index)%_rec_max):i]);
    }

    Timestamp *get_timestamp_i(int i) {
      return &(_rec_times[(_rec_index>=_rec_max)?((i+_rec_index)%_rec_max):i]);
    }
  };

  typedef HashMap<String,HandlerRecord *> HandlerRecordMap;
  typedef HandlerRecordMap::const_iterator HandlerRecordMapIter;

 public:
  BrnCompoundHandler();
  virtual ~BrnCompoundHandler();

 const char *class_name() const  { return "BrnCompoundHandler"; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void run_timer(Timer *);

  void add_handlers();

  void set_value( const String& value, ErrorHandler *errh );
  String read_handler();
  String handler();
  int handler_operation(const String &in_s, void *vparam, ErrorHandler *errh);

 private:

  String _handler;
  String _classes;
  String _classes_handler;
  String _classes_value;

  Vector<String> _vec_handlers;

  Vector<Element*> _vec_elements;
  Vector<const Handler*> _vec_elements_handlers;

  int _update_mode;
  HashMap<String,String> _last_handler_value;

  HandlerRecordMap _record_handler;
  int _record_mode;
  int _record_samples;
  int _sample_time;
  Timer _record_timer;
};

CLICK_ENDDECLS
#endif /*BrnCompoundHANDLER_HH_*/
