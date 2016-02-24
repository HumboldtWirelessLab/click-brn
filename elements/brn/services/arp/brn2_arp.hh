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

#ifndef CLICK_BRN2ARP_HH
#define CLICK_BRN2ARP_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>

#include "elements/brn/vlan/brn2vlantable.hh"
#include "../dhcp/dhcpsubnetlist.hh"
#include "elements/brn/dht/storage/dhtstorage.hh"

CLICK_DECLS

/**
 * BRN2Arp stuff
 */
class BRN2Arp : public Element {

 public:
  class ARPrequest {

   public:

   uint8_t _s_hw_add[6];
   uint8_t _s_ip_add[4];
   uint8_t _d_ip_add[4];

   uint8_t  _id;

   long _time;

   ARPrequest(const uint8_t *s_hw_add, const uint8_t *s_ip_add, const uint8_t *d_ip_add, uint8_t id, long time)
   {
     memcpy(_s_hw_add,s_hw_add,6);
     memcpy(_s_ip_add,s_ip_add,4);
     memcpy(_d_ip_add,d_ip_add,4);
     _id = id;
     _time = time;
   }

   ~ARPrequest()
   {}

  };

  //
  //methods
  //
  BRN2Arp();
  ~BRN2Arp();

  const char *class_name() const	{ return "BRN2Arp"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const        { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  void add_handlers();

  int initialize(ErrorHandler *);
  void cleanup(CleanupStage stage);

  void push( int port, Packet *packet );

  int _debug;

  void send_gratious_arp();

  int handle_dht_reply(DHTOperation *op);

 private:
  Vector<ARPrequest> request_queue;

  int handle_arp_request(Packet *p);

  int send_arp_reply( const uint8_t *s_hw_add, const uint8_t *s_ip_add, const uint8_t *d_hw_add, const uint8_t *d_ip_add );
  int send_arp_request( const uint8_t *s_hw_add, const uint8_t *s_ip_add, const uint8_t *d_hw_add, const uint8_t *d_ip_add );

  void dht_request(DHTOperation *op);

  IPAddress _router_ip;
  EtherAddress _router_ethernet;

  int count_arp_reply;

  IPAddress _src_ip;
  IPAddress _src_ip_mask;

  BRN2DHCPSubnetList *_dhcpsubnetlist;
  BRN2VLANTable *_vlantable;
  DHTStorage *_dht_storage;

};

CLICK_ENDDECLS
#endif
