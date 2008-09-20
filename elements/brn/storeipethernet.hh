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

#ifndef BRNSTOREIPETHERNETELEMENT_HH
#define BRNSTOREIPETHERNETELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <elements/wifi/arptable.hh>

CLICK_DECLS
/*
 * =c
 * StoreIPEthernet()
 * =s 
 * learns and stores ip/ethernet address combinations from incoming packets.
 * =d
 */
class StoreIPEthernet : public Element {

 public:
  //
  //methods
  //
  StoreIPEthernet();
  ~StoreIPEthernet();

  const char *class_name() const	{ return "StoreIPEthernet"; }
  const char *port_count() const	{ return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet *simple_action(Packet *);

  int initialize(ErrorHandler *);
  void add_handlers();

  static void print(Packet *p_in);

 public:
  int _debug;

 private:
  ARPTable *_arp_table;
};

CLICK_ENDDECLS
#endif
