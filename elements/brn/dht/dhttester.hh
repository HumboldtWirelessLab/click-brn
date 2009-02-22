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

#ifndef CLICK_DHTTESTER_HH
#define CLICK_DHTTESTER_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>

CLICK_DECLS

class DHTTester : public Element {

 public:
  //
  //methods
  //
  DHTTester();
  ~DHTTester();

  const char *class_name() const	{ return "DHTTester"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const        { return "1/1"; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  void add_handlers();

  int initialize(ErrorHandler *);

  void push( int port, Packet *packet );
  
  void run_timer();

  int _debug;

  uint32_t _request_number;

  int dht_request(void);
  int dht_answer(Packet *dht_p_in);
  
 private:

  Timer request_timer;

};

CLICK_ENDDECLS
#endif
