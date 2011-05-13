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

#ifndef CLICK_FILTERBSSID_HH
#define CLICK_FILTERBSSID_HH
#include <click/element.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

/*
=c

FilterBSSID

=s 

Filter out packets, which do not belong to the specified bssid using the WirelessInfo 

=d 

*/

class FilterBSSID : public BRNElement { public:

  FilterBSSID();
  ~FilterBSSID();

  const char *class_name() const	{ return "FilterBSSID"; }
  const char *port_count() const	{ return "1/1-3"; }
  const char *processing() const	{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  void add_handlers();

  void push(int port, Packet *p);

  bool _active;
  class WirelessInfo *_winfo;
};

CLICK_ENDDECLS
#endif
