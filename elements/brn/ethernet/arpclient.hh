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

#ifndef CLICK_ARPCLIENT_HH
#define CLICK_ARPCLIENT_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/timer.hh>

CLICK_DECLS


#define ARP_CLIENT_DEBUG
#define ARP_CLIENT_INFO

/*
 * ARPClient stuff
 */
class ARPClient : public Element {

 public:
  class ARPClientRequest {

   public:

   IPAddress ip_add;

   struct timeval _time_start;

   ARPClientRequest( int _ip )
   {
     _time_start = Timestamp::now().timeval();
     ip_add = IPAddress(_ip);
   }

   ~ARPClientRequest()
   {}

  };

  //
  //methods
  //
  ARPClient();
  ~ARPClient();

  const char *class_name() const  { return "ARPClient"; }
  const char *processing() const  { return PUSH; }

  const char *port_count() const  { return "1/1"; } 

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  void add_handlers();

  int initialize(ErrorHandler *);
  virtual void cleanup(CleanupStage stage);

  void run_timer();

  void push( int port, Packet *packet );

 private:
  int arp_reply(Packet *p);
  int send_arp_request( uint8_t *d_ip_add );

  long diff_in_ms(timeval t1, timeval t2);

// Data
public:
  int _debug;
  IPAddress _client_ip;
  EtherAddress _client_ethernet;

  Vector<ARPClientRequest> _request_queue;

  IPAddress _start_range;
  uint32_t _range;

  uint32_t _range_index;

  int _count_request;
  int _count_reply;
  int _count_timeout;

  Timer _request_timer;
  int _client_start;
  int _client_interval;
  int _count;
  int _requests_at_once;
  int _timeout;
  bool _active;
};

CLICK_ENDDECLS
#endif
