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

#ifndef DISASSOCIATORELEMENT_HH
#define DISASSOCIATORELEMENT_HH

#include <click/etheraddress.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>

CLICK_DECLS

class BRNAssocResponder;
class BrnIappStationTracker;

/*
 * =c
 * Disassociator(BRNAssocitationResponder ar, Disassociator stations)

 * =s Causes the BRNAssocitationResponder ar to send a disassoc to the station, if
 *    the station isn't associated (known from Disassociator stations)
 * =d
 */
class Disassociator : public Element {
  //
  //methods
  //
public:
  Disassociator();
  ~Disassociator();

  const char *class_name() const	{ return "Disassociator"; }
  const char *port_count() const	{ return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet *simple_action(Packet *);
  int initialize(ErrorHandler *);
  void add_handlers();

  int _debug;

private:
  //
  //member
  //
  BrnIappStationTracker*_sta_tracker;
  BRNAssocResponder*    _assocresp;
};

CLICK_ENDDECLS
#endif
