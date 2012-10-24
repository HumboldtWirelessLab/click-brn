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

#ifndef TIMESYNCINFOELEMENT_HH
#define TIMESYNCINFOELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/userutils.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * TimeSyncInfo()
 * =s debugging
 * shows information about the system
 * =d
 */

#define TIMESYNCINFO_DEFAULT_MAX_IDS 10

class TimeSyncInfo : public BRNElement {

 public:
  //
  //methods
  //
  TimeSyncInfo();
  ~TimeSyncInfo();

  const char *class_name() const	{ return "TimeSyncInfo"; }
  const char *processing() const	{ return AGNOSTIC; }

  const char *port_count() const  { return "1/0-1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  void push(int, Packet *p);

  String read_syncinfo();

 private:
  //
  //member
  //

   Timestamp *_timestamps;
   int32_t   *_packet_ids;

   uint32_t _max_ids;
   uint32_t _next_id;
};

CLICK_ENDDECLS
#endif
