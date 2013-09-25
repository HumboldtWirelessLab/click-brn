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

#ifndef BRN2DHTUSERSPACEELEMENT_HH
#define BRN2DHTUSERSPACEELEMENT_HH

#include <click/etheraddress.hh>
#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn/dht/storage/dhtstorage.hh"

#define MODE_WRITE  0
#define MODE_READ   1
#define MODE_REMOVE 2


CLICK_DECLS


/*
 * =c
 * DHTUserspace()
 * =s
 *
 * =d
 */

struct dht_op_header {
  uint8_t op;
  uint8_t status;
  uint16_t key_len;
  uint16_t value_len;
};

class DHTUserspace : public Element {

 public:
  //
  //methods
  //

  class UserspaceClientInfo {
   public:
    uint32_t _id;

    IPAddress _ip;
    uint16_t _port;

    Packet *_client_packet;

    UserspaceClientInfo(Packet *p, IPAddress ip, uint16_t port)
    {
      _ip = ip;
      _port = port;
      _client_packet = p;
      _id = 0;
    }

    ~UserspaceClientInfo()
    {
      if ( _client_packet != NULL ) _client_packet->kill();
    }

  };

/* dhcpserver.cc**/

  DHTUserspace();
  ~DHTUserspace();

  const char *class_name() const	{ return "DHTUserspace"; }
  const char *processing() const	{ return PUSH; }

  const char *port_count() const        { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }

  void push( int port, Packet *packet );

  int initialize(ErrorHandler *);
  void add_handlers();

 private:

  Vector<UserspaceClientInfo *> client_info_list;

 public:
  void dht_request(UserspaceClientInfo *client_info, DHTOperation *op);

  void handle_dht_reply(UserspaceClientInfo *client_info, DHTOperation *op);

 public:
  DHTUserspace::UserspaceClientInfo *get_client_by_dht_id(uint32_t id);
  int remove_client(UserspaceClientInfo *client_info);

  //
  //member
  //
 public:
  int _debug;

 private:

  DHTStorage *_dht_storage;
};

CLICK_ENDDECLS
#endif
