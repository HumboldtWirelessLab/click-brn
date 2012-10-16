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

#ifndef BRN2DNSCLIENTELEMENT_HH
#define BRN2DNSCLIENTELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/vector.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * BRN2DNSClient()
 * =s
 * SEND DHCPDISCOVER,...
 * =d
 */

#define DHCP_CLIENT_DEBUG
#define DHCP_CLIENT_INFO


class BRN2DNSClient : public BRNElement {

 public:

   class DNSClientInfo {

   public:

    String _hostname;
    IPAddress _addr;

    DNSClientInfo(String hostname)
    {
      _hostname = hostname;
    }

    ~DNSClientInfo()
    {}
  };

  //
  //methods
  //

/* dhcpclient.cc**/

  BRN2DNSClient();
  ~BRN2DNSClient();

  const char *class_name() const  { return "BRN2DNSClient"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  void run_timer(Timer*);

  int initialize(ErrorHandler *);
  virtual void cleanup(CleanupStage stage);

  void add_handlers();
  void init_state();

  String print_stats();

 private:

  Vector<DNSClientInfo> request_queue;

  Packet *dns_query(DNSClientInfo *client_info);

  int search_dnsclient_by_name(String name);

public:
  int _interval;
  int _start_time;
  String _domain;
  Timer _timer;
  IPAddress _ip;

  uint16_t transid;
  bool _active;

  /*stats*/
  bool _running_request;
  Timestamp _last_request;
  int _no_requests;
  int _no_replies;
  int _sum_request_time;
};

CLICK_ENDDECLS
#endif
