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

#ifndef BRN2ASSOCREQUESTER_HH_
#define BRN2ASSOCREQUESTER_HH_
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

#include "elements/brn2/wifi/brnavailablerates.hh"

// Why are the click guys not able to deliver proper headers?
CLICK_DECLS
class WirelessInfo;
CLICK_ENDDECLS

#include <elements/wifi/station/associationrequester.hh>

CLICK_DECLS

class BRN2AssocRequester : public AssociationRequester
{
//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------
public:
  BRN2AssocRequester();
  virtual ~BRN2AssocRequester();

//------------------------------------------------------------------------------
// Overrides
//------------------------------------------------------------------------------
public:
  const char *class_name() const  { return "BRN2AssocRequester"; }
  const char *port_count() const  { return PORTS_1_1; }
  const char *processing() const  { return PUSH; }

  bool can_live_reconfigure() const { return false; }

  int configure(Vector<String> &, ErrorHandler *);

  void push(int, Packet *);

  void add_handlers();

//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------
public:
  void send_assoc_req();
  void send_reassoc_req();

  void process_reassoc_resp(Packet *p);
  void process_response(Packet *p); 

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------
public:
  int _debug;
  EtherAddress _bssid;

  BrnAvailableRates *_rtable;
};

CLICK_ENDDECLS
#endif /*BRNASSOCREQUESTER_HH_*/
