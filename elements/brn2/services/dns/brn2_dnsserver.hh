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

#ifndef BRN2DNSSERVERELEMENT_HH
#define BRN2DNSSERVERELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>
#include "../../../brn/vlan/vlantable.hh"
#include "elements/brn/services/dhcp/dhcp.h"
#include "dnsconfiglist.hh"

#include "elements/brn2/dht/storage/dhtstorage.hh"

#define MODE_WRITE  0
#define MODE_READ   1
#define MODE_REMOVE 2


CLICK_DECLS


/*
 * =c
 * BRN2DNSServer()
 * =s
 * responds to dhcp requests
 * =d
 */

//#define NO_DHT

class BRN2DNSServer : public Element {

 public:
  //
  //methods
  //

  class DNSClientInfo {
   public:
    uint32_t _id;

    unsigned char _chaddr[6];
    struct in_addr _ciaddr;

    Packet *_client_packet;

    DNSClientInfo()
    {
      memcpy(&(_chaddr),"\0\0\0\0\0\0",6);
      memcpy(&(_ciaddr),"\0\0\0\0",4);
    }

    ~DNSClientInfo()
    {
    }

  };

/* dhcpserver.cc**/

  BRN2DNSServer();
  ~BRN2DNSServer();

  const char *class_name() const	{ return "BRN2DNSServer"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const        { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  String server_info(void);

 private:

  Vector<DNSClientInfo *> client_info_list;

 public:
  int send_dht_request(DNSClientInfo *client_info);
  void dht_request(DNSClientInfo *client_info, DHTOperation *op);

  void handle_dht_reply(DNSClientInfo *client_info, DHTOperation *op);

 public:
  BRN2DNSServer::DNSClientInfo *get_client_by_dht_id(uint32_t id);                              //TODO: remove
  int remove_client(DNSClientInfo *client_info);

//  uint16_t getVlanID(EtherAddress ea);

  //
  //member
  //
public:
  int _debug;

private:
  EtherAddress _me;


  IPAddress _router;             //IP
  IPAddress _server_ident;       //IP DHCP-Server
  IPAddress _name_server;        //IP

  String _domain_name;
  String _sname;           //servername
  String _full_sname;

  BRN2DNSConfigList *_dhcpsubnetlist;
  VLANTable *_vlantable;
  DHTStorage *_dht_storage;
};

CLICK_ENDDECLS
#endif
