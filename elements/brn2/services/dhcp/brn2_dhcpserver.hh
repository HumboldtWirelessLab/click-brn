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

#ifndef BRN2DHCPSERVERELEMENT_HH
#define BRN2DHCPSERVERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "../../../brn/vlan/vlantable.hh"
#include "dhcp.h"
#include "dhcpsubnetlist.hh"

#include "elements/brn2/dht/storage/dhtstorage.hh"

#define MODE_WRITE_IP  0
#define MODE_READ_IP   1
#define MODE_REMOVE_IP 2

#define MODE_WRITE_NAME  3
#define MODE_READ_NAME   4
#define MODE_REMOVE_NAME 5


CLICK_DECLS


/*
 * =c
 * BRN2DHCPServer()
 * =s
 * responds to dhcp requests
 * =d
 */

//#define NO_DHT

class BRN2DHCPServer : public Element {

 public:
  //
  //methods
  //

  class DHCPClientInfo {
   public:
    uint8_t _status;

    uint8_t _dht_op;                          //TODO: check ( puttogether with _status ?? )
    uint32_t _id;

    unsigned char _chaddr[6];
    struct in_addr _ciaddr;

    String name;

    uint32_t _xid;
    bool _broadcast;

    uint32_t _lease_time;
    uint32_t _old_lease_time;

    Packet *_client_packet;

    DHCPClientInfo()
    {
      memcpy(&(_chaddr),"\0\0\0\0\0\0",6);
      memcpy(&(_ciaddr),"\0\0\0\0",4);
    }

    ~DHCPClientInfo()
    {
    }

  };

/* dhcpserver.cc**/

  BRN2DHCPServer();
  ~BRN2DHCPServer();

  const char *class_name() const	{ return "BRN2DHCPServer"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const        { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  String server_info(void);

 private:

  Vector<DHCPClientInfo *> client_info_list;

  int handle_dhcp_discover(Packet *p_in);
  int handle_dhcp_request(Packet *p_in);
  int handle_dhcp_release(Packet *p_in);
  int handle_dhcp_decline(Packet *p_in);
  int handle_dhcp_inform(Packet *p_in);

  int send_dhcp_ack(DHCPClientInfo *client_info, uint8_t messagetype);
  int send_dhcp_offer(DHCPClientInfo *client_info);

  void dhcp_add_standard_options(WritablePacket *p);

 public:
  int send_dht_request(DHCPClientInfo *client_info);
  void dht_request(DHCPClientInfo *client_info, DHTOperation *op);

  void handle_dht_reply(DHCPClientInfo *client_info, DHTOperation *op);

 public:
  BRN2DHCPServer::DHCPClientInfo *get_client_by_mac(uint8_t *mac);
  BRN2DHCPServer::DHCPClientInfo *get_client_by_dht_id(uint32_t id);                              //TODO: remove
  int remove_client(DHCPClientInfo *client_info);

  void find_client_ip(DHCPClientInfo *client_info, uint16_t vlan_id);
  uint32_t find_lease(void);
  uint16_t getVlanID(EtherAddress ea);

  //
  //member
  //
public:
  int _debug;

private:
  EtherAddress _me;
  IPAddress start_ip_range;

  IPAddress _net_address;
  IPAddress _subnet_mask;
  IPAddress _broadcast_address;

  IPAddress _router;             //IP
  IPAddress _server_ident;       //IP DHCP-Server
  IPAddress _name_server;        //IP

  String _domain_name;

  String _sname;           //servername
  String _bootfilename;    //name des Bootfiles

  String _time_offset;

  uint32_t _default_lease;

  //DEBUG
  int debug_count_dhcp_packet;
  int debug_count_dht_packets;

  BRN2DHCPSubnetList *_dhcpsubnetlist;
  VLANTable *_vlantable;
  DHTStorage *_dht_storage;
};

CLICK_ENDDECLS
#endif
