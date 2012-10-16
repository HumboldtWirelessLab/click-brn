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

#ifndef BRN2DHCPCLIENTELEMENT_HH
#define BRN2DHCPCLIENTELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/timer.hh>
#include <click/vector.hh>
#include "dhcp.h"

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

/*
 * =c
 * BRN2DHCPClient()
 * =s
 * SEND DHCPDISCOVER,...
 * =d
 */

#define DHCP_CLIENT_DEBUG
#define DHCP_CLIENT_INFO


class BRN2DHCPClient : public BRNElement {

 public:

   int _debug;

   class DHCPClientInfo {

   public:

    EtherAddress eth_add;
    IPAddress ip_add;
    uint32_t xid;

    struct timeval _time_start;

    uint8_t _last_action;
    Timestamp _last_action_time;

    uint8_t status;
    uint32_t lease;

    IPAddress server_add;

    DHCPClientInfo(uint32_t _xid, uint8_t *_mac, int _ip )
    {
      xid =_xid;
      eth_add = EtherAddress(_mac);
      ip_add = IPAddress(_ip);

      _time_start = Timestamp::now().timeval();
      _last_action_time = Timestamp::now();

      _last_action = 0;

      status = DHCPINIT;
      _last_action = 0;
    }

    DHCPClientInfo(uint32_t _xid, uint8_t *_mac)
    {
      xid =_xid;
      eth_add = EtherAddress(_mac);

      _time_start = Timestamp::now().timeval();
      _last_action_time = Timestamp::now();

      _last_action = 0;

      status = DHCPINIT;
    }

    ~DHCPClientInfo()
    {}
  };

  //
  //methods
  //

/* dhcpclient.cc**/

  BRN2DHCPClient();
  ~BRN2DHCPClient();

  const char *class_name() const  { return "BRN2DHCPClient"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/2"; } 
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

  Vector<DHCPClientInfo> request_queue;

  Packet *dhcpdiscover(DHCPClientInfo *client_info);
  Packet *dhcprequest(DHCPClientInfo *client_info);
  void dhcpbound(DHCPClientInfo *client_info);
  Packet *dhcprenewing();
  Packet *dhcprebinding();
  Packet *dhcprelease(DHCPClientInfo *client_info);
  Packet *dhcpinform();

  int search_dhcpclient_by_xid(int xid);

public:
  EtherAddress _hw_addr;

  IPAddress _ip_addr;
  int _ip_range;

  int _start_time;
  int _interval;

  uint32_t range_index;

  Timer _timer;

  int count_configured_clients;
  bool _active;
};

CLICK_ENDDECLS
#endif
