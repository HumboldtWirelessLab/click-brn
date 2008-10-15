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

#ifndef CLICK_STATIONHANDOVER_HH
#define CLICK_STATIONHANDOVER_HH
#include <click/element.hh>
#include <click/ewma.hh>
#include <click/atomic.hh>
#include <click/timer.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>
#include "brnassocrequester.hh"

CLICK_DECLS

/*
 * =c
 * StationHandover
 * =s measures
 * Used by base stations to measure rssi values to nearby access points and to automatically
 * initiate reassociations (handover).
 * =d
 *
 */
class StationHandover : public Element {

 public:

  typedef Vector<uint8_t> RSSIVector;

  typedef HashMap<EtherAddress, RSSIVector> AccessPointMap;

  int _debug;
  bool _active;
  int _delta;
  int _rssi_min_diff;
  AccessPointMap *_ap_map;
  BRNAssocRequester *_station_assoc;
  class WirelessInfo *_winfo;

 public:

  StationHandover();
  ~StationHandover();

  const char *class_name() const		{ return "StationHandover"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH; }
  int configure(Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int, Packet *);

  EtherAddress retrieve_bssid(Packet *p, String required_ssid);
  void check_for_handover();
};

CLICK_ENDDECLS
#endif
