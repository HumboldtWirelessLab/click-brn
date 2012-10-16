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

#include "elements/brn2/brnelement.hh"

#include "elements/brn2/vlan/brn2vlantable.hh"
#include "elements/brn2/services/dhcp/dhcp.h"
#include "elements/brn2/services/dhcp/dhcpsubnetlist.hh"

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

class BRN2DNSServer : public BRNElement {

 public:
  //
  //methods
  //

  class DNSClientInfo {
   public:
    uint32_t _id;

    unsigned char _chaddr[6];
    IPAddress _ip;

    String _domain;

    Packet *_client_packet;

    DNSClientInfo()
    {
      memcpy(&(_chaddr),"\0\0\0\0\0\0",6);
    }

    DNSClientInfo(Packet *p,IPAddress ip, String domain)
    {
      _ip = ip;
      memcpy(&(_chaddr),"\0\0\0\0\0\0",6);
      _client_packet = p;
      _domain = domain;
      _id = 0;
    }

    ~DNSClientInfo()
    {
      if ( _client_packet != NULL ) _client_packet->kill();
    }

  };

/* dhcpserver.cc**/

  BRN2DNSServer();
  ~BRN2DNSServer();

  const char *class_name() const	{ return "BRN2DNSServer"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const        { return "1/2"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

  String server_info(void);

 private:

  Vector<DNSClientInfo *> client_info_list;

 public:
  void dht_request(DNSClientInfo *client_info, DHTOperation *op);

  void handle_dht_reply(DNSClientInfo *client_info, DHTOperation *op);

 public:
  BRN2DNSServer::DNSClientInfo *get_client_by_dht_id(uint32_t id);                              //TODO: remove
  int remove_client(DNSClientInfo *client_info);

//  uint16_t getVlanID(EtherAddress ea);

  //
  //member
  //
private:
  EtherAddress _me;


  IPAddress _router;             //IP
  IPAddress _server_ident;       //IP DHCP-Server
  IPAddress _name_server;        //IP

  String _domain_name;
  String _sname;           //servername
  String _full_sname;

  BRN2DHCPSubnetList *_dhcpsubnetlist;
  BRN2VLANTable *_vlantable;
  DHTStorage *_dht_storage;

  bool _server_redirect;
};

CLICK_ENDDECLS
#endif
