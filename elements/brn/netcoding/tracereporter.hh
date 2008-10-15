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

#ifndef TRACEREPORTER_HH_
#define TRACEREPORTER_HH_

#include <click/etheraddress.hh>
#include <click/element.hh>
#include "tracecollector.hh"



CLICK_DECLS	

enum TraceMode {FORWARD = 0, DUPE = 1, DROP = 2};

class TraceReporter : public Element {

 public:
  
  const char *class_name() const	{ return "TraceReporter"; }
  const char *port_count() const	{ return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }
  int configure(Vector<String> &, ErrorHandler *);
  Packet *simple_action(Packet *);
  
 private:
 
  TraceMode mode;
  TraceCollector * parent;
};

CLICK_ENDDECLS

#endif /*TRACEREPORTER_HH_*/
