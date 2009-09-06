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

#ifndef BRNBROADCASTROUTINGELEMENT_HH
#define BRNBROADCASTROUTINGELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"

CLICK_DECLS
/*
 * =c
 * BrnBroadcastRouting()
 * =s
 * Input 0  : Packets to route
 * Input 1  : BRNBroadcastRouting-Packets
 * Output 0 : BRNBroadcastRouting-Packets
 * Output 1 : Packets to local
  * =d
 */
#define MAX_QUEUE_SIZE  1500

struct click_bcast_routing_header {
  uint16_t      bcast_id;
  hwaddr        dsr_dst;
};

class BrnBroadcastRouting : public Element {

 public:

  class BrnBroadcast
  {
    public:
      uint16_t      bcast_id;
      uint8_t       _dst[6];
      uint8_t       _src[6];

      BrnBroadcast( uint16_t _id, uint8_t *src, uint8_t *dst )
      {
        bcast_id = _id;
        memcpy(&_src[0], src, 6);
        memcpy(&_dst[0], dst, 6);
      }

      ~BrnBroadcast()
      {}
  };

  //
  //methods
  //
  BrnBroadcastRouting();
  ~BrnBroadcastRouting();

  const char *class_name() const  { return "BrnBroadcastRouting"; }
  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "2/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  //
  //member
  //

  Vector<BrnBroadcast> bcast_queue;
  uint16_t bcast_id;
  EtherAddress _my_ether_addr;

 public:
  int _debug;

};

CLICK_ENDDECLS
#endif
