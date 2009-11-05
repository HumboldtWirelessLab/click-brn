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

#ifndef BRN2ETHERENCAP_HH
#define BRN2ETHERENCAP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

/*
 *=c
 *BRN2EtherEncap()
 *=s encapsulation, Ethernet
 *encapsulates packets in Ethernet header (information used from Packet::ether_header())
*/
class BRN2EtherEncap : public Element {

 public:
  //
  //methods
  //
  BRN2EtherEncap();
  ~BRN2EtherEncap();

  const char *class_name() const	{ return "BRN2EtherEncap"; }
  const char *port_count() const        { return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  Packet *smaction(Packet *);
  void push(int, Packet *);
  Packet *pull(int);

  bool _use_anno;
  int _debug;

  /*static functions*/
  static Packet *push_ether_header(Packet *p, uint8_t *src, uint8_t *dst, uint16_t ethertype);
};

CLICK_ENDDECLS
#endif
