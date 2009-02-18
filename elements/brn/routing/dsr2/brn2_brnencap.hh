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

#ifndef BRN2ENCAPELEMENT_HH
#define BRN2ENCAPELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * BRN2Encap()
 * =s prepends a brn header to a given packet
 * ...
 * =d
 */
class BRN2Encap : public Element {

 public:
  //
  //member
  //
  int _debug;

  //
  //methods
  //
  BRN2Encap();
  ~BRN2Encap();

  const char *class_name() const	{ return "BRN2Encap"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  int initialize(ErrorHandler *);

  Packet *add_brn_header(Packet *);
  Packet *add_brn_header(Packet *, unsigned int);
  void add_handlers();
};

CLICK_ENDDECLS
#endif