/* Copyright (C) 2008 Ulf Hermann
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
 */

#ifndef TRACECOLLECTOR_HH_
#define TRACECOLLECTOR_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <elements/brn/brn.h>
#include <click/straccum.hh>

CLICK_DECLS


	


class TraceCollector : public Element {

 public:
  
  const char *class_name() const	{ return "TraceCollector"; }
  const char *port_count() const	{ return "0/0"; }
  const char *processing() const	{ return AGNOSTIC; }

  void duplicate(const click_brn_dsr * dsrHeader, char pos);
  void forwarded(const click_brn_dsr * dsrHeader, char pos);
  void corrupt(const click_brn_dsr * dsrHeader, char pos);
  void add_handlers();

 private:
  
  EtherAddress getLastNode(const click_brn_dsr * dsrHeader, char pos);
  int _debug; 
  String _label;
  
  StringAccum forward;
  StringAccum dupe;
  StringAccum drop;
  
  static String read_handler(Element*, void*);
  
};

CLICK_ENDDECLS

#endif /*TRACECOLLECTOR_HH_*/
