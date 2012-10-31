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

#ifndef SEISMODETECTION_COOPERATIVE_ELEMENT_HH
#define SEISMODETECTION_COOPERATIVE_ELEMENT_HH
#include <click/element.hh>
#include "elements/brn/brnelement.hh"
#include "elements/brn/brn2.h"

#include "seismo_reporting.hh"

CLICK_DECLS

CLICK_SIZE_PACKED_STRUCTURE(
struct click_seismodetection_coop_header {,
  uint8_t flags;
  uint8_t no_alarms;
});

CLICK_SIZE_PACKED_STRUCTURE(
struct click_seismodetection_alarm_info {,
  uint32_t _start;
  uint32_t _end;
  uint16_t _id;
});

CLICK_SIZE_PACKED_STRUCTURE(
struct click_sdetect_coop_alg_info {,
  uint8_t algo;
  uint8_t size_of_alarm;  //if node can't handle the algo, it can ignore and remove the info
});

class SeismoDetectionCooperative : public BRNElement, public SeismoDetectionAlgorithm {

  public:

  SeismoDetectionCooperative();
  ~SeismoDetectionCooperative();

  const char *class_name() const  { return "SeismoDetectionCooperative"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "1/1"; } //0: local 1: remote

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);

  void add_handlers();

  void push(int, Packet *p);

  String stats();

  void update(SrcInfo *si, uint32_t next_new_block);
  SeismoAlarmList *get_alarm() { return NULL; };

  typedef HashMap<EtherAddress, SeismoAlarmList*> NodeAlarmMap;
  typedef NodeAlarmMap::const_iterator NodeAlarmMapIter;

  SeismoAlarmList _sal;
  SeismoDetectionAlgorithmList _sdal;
  NodeAlarmMap _salm;

  String _algo_string;

  uint16_t _max_alarm;

  Timestamp _last_packet;

};

CLICK_ENDDECLS
#endif
