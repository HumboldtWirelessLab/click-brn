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

#include "elements/brn/brnelement.hh"

#include "seismo.hh"

#define SEISMO_REPORT_DEFAULT_INTERVAL 500
#define SEISMO_REPORT_MAX_ALARM_COUNT  100

CLICK_DECLS

class SeismoAlarm {
  public:

    Timestamp _start;
    Timestamp _end;
    uint16_t  _id;

    void *_detection_info;

    SeismoAlarm() {
      _start = _end = Timestamp::now();
      _detection_info = NULL;
    }

    void end_alarm() {
      _end = Timestamp::now();
    }
};

typedef Vector<SeismoAlarm*> SeismoAlarmList;
typedef SeismoAlarmList::const_iterator SeismoAlarmListIter;

class SeismoDetectionAlgorithm {
 public:
  virtual void update(SrcInfo *sibl, uint32_t next_new_block) = 0;
  virtual SeismoAlarmList *get_alarm() = 0;
};

typedef Vector<SeismoDetectionAlgorithm*> SeismoDetectionAlgorithmList;
typedef SeismoDetectionAlgorithmList::const_iterator SeismoDetectionAlgorithmListIter;

class SeismoReporting : public BRNElement {

  public:

  SeismoReporting();
  ~SeismoReporting();

  const char *class_name() const  { return "SeismoReporting"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; } //0: local

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void run_timer(Timer *);

  void add_handlers();

  void seismo_evaluation();

  //String print_stats();
  String print_alarm();

  Timer _reporting_timer;

  Seismo *_seismo;
  SeismoAlarmList _sal;
  SeismoDetectionAlgorithmList _sdal;

  String _algo_string;

  uint32_t _next_block_id;

  uint32_t _interval;
};

CLICK_ENDDECLS
#endif
