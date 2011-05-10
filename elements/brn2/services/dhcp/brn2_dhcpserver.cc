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

/*
 * dhcpserver.{cc,hh} -- responds to dhcp requests
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/string.hh>

#include "brn2_dhcpserver.hh"
#include "elements/brn2/dht/storage/dhtoperation.hh"
#include "elements/brn2/dht/storage/dhtstorage.hh"
#include "elements/brn2/dht/storage/rangequery/ip_rangequery.hh"

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "dhcpprotocol.hh"

CLICK_DECLS

BRN2DHCPServer::BRN2DHCPServer() :
  _dhcpsubnetlist(NULL),
  _vlantable(NULL),
  _lease_table(NULL)
{
  BRNElement::init();
}

BRN2DHCPServer::~BRN2DHCPServer()
{
  if (_debug >= BrnLogger::INFO) {
    click_chatter("BRN2DHCPServer: end: free all");
  }

  for ( int i = 0; i < client_info_list.size(); i++ )
  {
    client_info_list[i]->_client_packet->kill();
    delete client_info_list[i];
    client_info_list.erase(client_info_list.begin() + i);
  }
}

int
BRN2DHCPServer::configure(Vector<String> &conf, ErrorHandler* errh)
{
    BRN_DEBUG("BRN2DHCPServer: Configure");

  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM, cpEthernetAddress, /*"EtherAddress",*/ &_me,
      "ADDRESSPREFIX", cpkP+cpkM, cpIPPrefix, /*"address prefix",*/ &_net_address, &_subnet_mask,   /* e.g. "10.9.0.0/16" */
      "ROUTER", cpkP+cpkM, cpIPAddress,/* "router",*/ &_router,                  /* e.g. "10.9.0.1" */
      "SERVER", cpkP+cpkM, cpIPAddress, /*"server_identifier",*/ &_server_ident, /* e.g. "10.9.0.1" */
      "DNS", cpkP+cpkM, cpIPAddress, /*"name server",*/ &_name_server,        /* e.g. "141.20.20.50" */
      "SERVERNAME", cpkP+cpkM, cpString, /*"servername",*/ &_sname,                  /* e.g. "dhcp.brn.net" */
      "DOMAIN", cpkP+cpkM, cpString, /*"domainname",*/ &_domain_name,            /* e.g. "brn.net" */
      "DHTSTORAGE", cpkP+cpkM, cpElement, &_dht_storage,
      "DHCPSUBNETLIST", cpkP, cpElement, &_dhcpsubnetlist,
      "VLANTABLE", cpkP, cpElement, &_vlantable,
      "LEASETABLE", cpkP, cpElement, &_lease_table,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
BRN2DHCPServer::initialize(ErrorHandler *)
{
  _default_lease = 3600;
  debug_count_dhcp_packet = 0;

//  BRN_DEBUG("Configuration %s: Address space %s/%s, Router %s, ID %s, DNS %s, "
//    "Name %s, Domain %s", _me.unparse().c_str(), _net_address.unparse().c_str(),
//    _subnet_mask.unparse().c_str(), _router.unparse().c_str(), _server_ident.unparse().c_str(),
//    _name_server.unparse().c_str(), _sname.c_str(), _domain_name.c_str());

  return 0;
}

static void callback_func(void *e, DHTOperation *op)
{
  BRN2DHCPServer *s = (BRN2DHCPServer *)e;

  BRN2DHCPServer::DHCPClientInfo *client_info = s->get_client_by_dht_id(op->get_id());

  if ( client_info != NULL ) {
    s->handle_dht_reply(client_info,op);
  } else {
    click_chatter("DHCPServer: No Client info for DHT-ID. ID=%d",op->get_id());
    delete op;
  }
}

void
BRN2DHCPServer::dht_request(DHCPClientInfo *client_info, DHTOperation *op)
{
  //click_chatter("send storage request");
  uint32_t result = _dht_storage->dht_request(op, callback_func, (void*)this );

  if ( result == 0 )
  {
    BRN_DEBUG("Got direct-reply (local)");
    handle_dht_reply(client_info,op);
  } else {
    client_info->_id = result;
  }
}

void
BRN2DHCPServer::handle_dht_reply(DHCPClientInfo *client_info, DHTOperation *op)
{
  int result;

  BRN_DEBUG("BRN2DHCPServer: Handle DHT-Answer");
  BRN_DEBUG("BRN2DHCPServer: Client_status: %d",client_info->_status);

  switch ( client_info->_status )
  {
    case DHCPINIT:
    {
      if ( client_info->_dht_op == MODE_READ_IP )
      {
        if ( ( op->header.status == DHT_STATUS_KEY_NOT_FOUND ) ||
             ( ( op->header.status == DHT_STATUS_OK ) && ( op->header.valuelen == 6 ) && ( memcmp(op->value, client_info->_chaddr, 6 ) == 0 ) ) )
        {
          BRN_DEBUG("BRN2DHCPServer: IP/Mac is not used or used by the Client !!!");
          result = send_dhcp_offer(client_info);
          delete op;
        }
        else
        {
          BRN_DEBUG("BRN2DHCPServer: IP/Mac used by another Client");

          uint16_t vlanid = getVlanID(EtherAddress(client_info->_chaddr));
          if ( vlanid != 0xFFFF ) {
            find_client_ip( client_info, vlanid );
            delete op;
            result = send_dht_request(client_info);
           // return(result);
          } else {
            click_chatter("Client has no vlan");
            result = 1;
            delete op;
            //TODO: remove client info
          }
        }
      }
      else
      {
        BRN_ERROR("UNKNOWN DHT-OPERATION FOR DHCPINIT");
        delete op;
        result = 1;
      }

      break;
    }
    case DHCPINITREBOOT:
    case DHCPSELECTING:
    case DHCPRENEWING:
    case DHCPREBINDING:
    {
      switch ( client_info->_dht_op )
      {
        case MODE_READ_IP:
        {
          if ( ( op->header.status == DHT_STATUS_KEY_NOT_FOUND ) ||
                 ( ( op->header.status == DHT_STATUS_OK ) && ( op->header.valuelen == 6 ) && ( memcmp(op->value, client_info->_chaddr, 6 ) == 0 ) ) )
          {
            BRN_DEBUG("BRN2DHCPServer: IP is not used or used by this client. Write new MAC to DHT!");

            client_info->_dht_op = MODE_WRITE_IP;
            delete op;

            result = send_dht_request(client_info);
        //    return(result);
          }
          else
          {
            BRN_DEBUG("BRN2DHCPServer: IP is used by another Client");
            delete op;
            result = send_dhcp_ack(client_info,DHCPNAK );
          }
          break;
        }
        case MODE_WRITE_IP:
        {
          BRN_DEBUG("BRN2DHCPServer: Read NAME from DHT");
          if ( op->header.status == DHT_STATUS_OK ) //all O.K.
          {
            client_info->_dht_op = MODE_READ_NAME;

            DHTOperation *op_new = new DHTOperation();
            char hostname[255];
            sprintf(hostname,".%s%s",IPAddress(client_info->_ciaddr).unparse().c_str(),_domain_name.c_str());
            op_new->read((uint8_t*) hostname, strlen(hostname));

            dht_request(client_info,op_new);
          }
          else
          {
            BRN_ERROR("BRN2DHCPServer: ERROR: COULDN'T READ FROM DHT");
            result = send_dhcp_ack(client_info, DHCPNAK );
          }
          delete op;
          break;
        }
        case MODE_READ_NAME:
        {
          BRN_DEBUG("BRN2DHCPServer: Write NAME to DHT (read)");
          if ( ( op->header.status == DHT_STATUS_KEY_NOT_FOUND ) )
          //|| ( ( op->header.status == DHT_STATUS_OK ) && ( op->header.valuelen ==  ) && ( memcmp(op->value, client_info->_chaddr, 6 ) == 0 ) ) )
          {
            client_info->_dht_op = MODE_WRITE_NAME;

            DHTOperation *op_new = new DHTOperation();
            char hostname[255];
            sprintf(hostname,".%s%s",IPAddress(client_info->_ciaddr).unparse().c_str(),_domain_name.c_str());
            op_new->insert((uint8_t*) hostname, strlen(hostname), (uint8_t*)&client_info->_ciaddr,4 );

            dht_request(client_info,op_new);
          }
          else
          {
            BRN_ERROR("BRN2DHCPServer: ERROR: COULDN'T WRITE TO DHT");
            result = send_dhcp_ack(client_info, DHCPNAK );
          }
          delete op;
          break;
        }
        case MODE_WRITE_NAME:
        {
          BRN_DEBUG("BRN2DHCPServer: Wrote NAME to DHT");
          if ( op->header.status == DHT_STATUS_OK ) //all O.K.
          {
            result = send_dhcp_ack(client_info, DHCPACK );
          }
          else
          {
            BRN_ERROR("BRN2DHCPServer: ERROR: COULDN'T WRITE TO DHT");
            result = send_dhcp_ack(client_info, DHCPNAK );
          }
          delete op;
          break;
        }
        default:
        {
          BRN_ERROR("BRN2DHCPServer: ERROR: UNKNOWN DHT-OPERATION");
          result = 1;
          delete op;
          break;
        }
      }
      break;
    }
    case DHCPBOUND:
    {
      if ( client_info->_dht_op == MODE_REMOVE_IP )
      {
        BRN_DEBUG("BRN2DHCPServer: Client wurde entfernt");
        client_info->_client_packet->kill();
        debug_count_dhcp_packet--;
        result = remove_client(client_info);
        delete op;
      }
      break;
    }
    default:
    {
      BRN_DEBUG("BRN2DHCPServer: dht-answer unknown client status !");
      result = 1;
      delete op;
      break;
    }
  }
}

//Frage von client -> frage an dht -> antwort von dht -> antwort für client

void
BRN2DHCPServer::push( int port, Packet *p_in )
{
  int result = -1;

  BRN_DEBUG("BRN2DHCPServer: PUSH an Port %d",port);

  if ( port == 0 )
  {
    debug_count_dhcp_packet++;

    int packet_type = DHCPProtocol::retrieve_dhcptype(p_in);

    switch (packet_type) {
      case DHCPDISCOVER:
        result = handle_dhcp_discover(p_in); // ask dht for ip
        break;
      case DHCPREQUEST:
        result = handle_dhcp_request(p_in);  // send ack or nack to Client
        break;
      case DHCPRELEASE:
        result = handle_dhcp_release(p_in);  // send ip to dht
        break;
      case DHCPDECLINE:
        result = handle_dhcp_decline(p_in);  // client say: ip already in use
        break;
      case DHCPINFORM:
        result = handle_dhcp_inform(p_in);   // send info only to client
        break;
      case DHCPACK:         //sendet der Server 
      case DHCPNAK:         //sendet der Server 
      case DHCPOFFER:       //sendet der Server 
        break;
      default:
        {
          BRN_DEBUG("BRN2DHCPServer: unknown packet type!!!");
          break;
        }
    }
  }
}

int
BRN2DHCPServer::send_dht_request(DHCPClientInfo *client_info)
{
  BRN_DEBUG("BRN2DHCPServer: Packet for DHT");

  DHTOperation *op;

  switch ( client_info->_status )
  {
    case DHCPINIT:
    {
      BRN_DEBUG("BRN2DHCPServer: Clientstatus: DHCPINIT");
      BRN_DEBUG("BRN2DHCPServer: Send packet to DHT");

      op = new DHTOperation();
      op->read((uint8_t*) &client_info->_ciaddr, 4);

      if ( _dht_storage->range_query_support() ) {
        uint8_t key_id[16];
        IPRangeQuery iprq = IPRangeQuery(client_info->_subnet_ip, client_info->_subnet_mask);
        iprq.get_id_for_value(IPAddress(client_info->_ciaddr), key_id);
        op->set_key_digest(key_id);
      }

      dht_request(client_info,op);

      break;
    }
    case DHCPINITREBOOT:
    case DHCPSELECTING:
    case DHCPRENEWING:
    case DHCPREBINDING:
    {
      if ( client_info->_dht_op == MODE_READ_IP )
      {
        BRN_DEBUG("BRN2DHCPServer: Clientstatus: DHCPSELECTING READ");

        op = new DHTOperation();
        op->read((uint8_t*) &client_info->_ciaddr, 4);
      }
      else if ( client_info->_dht_op == MODE_WRITE_IP )
      {
        BRN_DEBUG("BRN2DHCPServer: Clientstatus: DHCPSELECTING WRITE");

        op = new DHTOperation();
        op->insert((uint8_t*) &client_info->_ciaddr, 4, (uint8_t*) &client_info->_chaddr, 6);
      }
      else
      {
        BRN_ERROR("BRN2DHCPServer: send_dht_question: ERROR: UNKNOWN DHT-OPERATION");

        client_info->_client_packet->kill();
        remove_client(client_info);
        return( -1 );
      }

      if ( _dht_storage->range_query_support() ) {
        uint8_t key_id[16];
        IPRangeQuery iprq = IPRangeQuery(client_info->_subnet_ip, client_info->_subnet_mask);
        iprq.get_id_for_value(IPAddress(client_info->_ciaddr), key_id);
        op->set_key_digest(key_id);
      }

      BRN_DEBUG("BRN2DHCPServer: Send packet to DHT");
      dht_request(client_info,op);

      break;
    }
    default:
      {
        BRN_DEBUG("BRN2DHCPServer: send_dht_question: unknown client status !");
        break;
      }
   }

   return(0);
}

// Client: DHCP-Server suchen   ( p_in )
// Server: IP-Anbieten          ( Packet )

int
BRN2DHCPServer::handle_dhcp_discover(Packet *p_in)
{
  BRN_DEBUG("BRN2DHCPServer: Clientstatus: INIT!");
  BRN_DEBUG("BRN2DHCPServer: Handle DHCP-Discover");

  int result;

  DHCPClientInfo *client_info;

  struct dhcp_packet *new_dhcp_packet = (struct dhcp_packet *)p_in->data();

  client_info = (DHCPClientInfo *)get_client_by_mac(new_dhcp_packet->chaddr);

  if ( client_info == NULL )
  {
    BRN_DEBUG("BRN2DHCPServer: Client not in queue! Create new one.");

    client_info = new DHCPClientInfo();
  }
  else
  {
    BRN_DEBUG("BRN2DHCPServer: ungeduldiger Client ! Ignoriere erneutes DHCPDiscover");

    p_in->kill();
    debug_count_dhcp_packet--;
    return(0);
  }

  client_info->_status = DHCPINIT;
  client_info->_dht_op = MODE_READ_IP;
  client_info->_broadcast = true;
  client_info->_xid = new_dhcp_packet->xid;
  memcpy(client_info->_chaddr, new_dhcp_packet->chaddr, 6);

  client_info->_client_packet = p_in;

  int op_size;
  unsigned char *requested_ip = DHCPProtocol::getOption(p_in, DHO_DHCP_REQUESTED_ADDRESS ,&op_size);

  if ( ( requested_ip == NULL ) && ( memcmp(&new_dhcp_packet->ciaddr,"\0\0\0\0",4) == 0 ) )
  {
    memcpy(&(client_info->_ciaddr),"\0\0\0\0",4);
    uint16_t vlanid = getVlanID(EtherAddress(client_info->_chaddr));

    BRN_DEBUG("Client %s has VLANID %d",EtherAddress(client_info->_chaddr).unparse().c_str(),vlanid);

    if ( vlanid != 0xFFFF ) {
      find_client_ip(client_info, vlanid);
    } else {
      return 1;  //ERROR. TODO: better Error-Handling
    }
  }
  else
  {
    BRN_DEBUG("BRN2DHCPServer: INIT. Client want special IP");

    IPAddress req_ip;

    if ( requested_ip != NULL )
      req_ip = IPAddress(requested_ip);
    else
      req_ip = IPAddress(new_dhcp_packet->ciaddr);

    bool wanted_ip_ok = (req_ip.addr() & _subnet_mask.addr()) != (_net_address.addr() & _subnet_mask.addr());

    if ( _lease_table ) {
      Lease *lease = _lease_table->lookup(req_ip);
      if ( lease != NULL ) {
        if ( memcmp(client_info->_chaddr, lease->_eth.data(), 6) != 0 ) {  //ip used by other eth
          wanted_ip_ok = false;
        }
      }
    }

    uint16_t vlanid = 0;
    IPAddress req_subnet_mask;
    IPAddress req_net_address;

    if (  _vlantable != NULL ) {
      vlanid = _vlantable->find(EtherAddress(new_dhcp_packet->chaddr));
      if ( vlanid != 0xFFFF ) {
        wanted_ip_ok = wanted_ip_ok && ( vlanid == 0 );                   //major subnet alway mapped to vlan0
        if ( wanted_ip_ok ) {
          req_subnet_mask = _subnet_mask;
          req_net_address = _net_address;
        }
      } else {
        return 1; //ERROR. TODO: better Error-Handling
      }
    }

    if ( ( _dhcpsubnetlist != NULL ) && ( ! wanted_ip_ok ) ) {
      BRN2DHCPSubnetList::DHCPSubnet *sn;
      for ( int i = 0; i < _dhcpsubnetlist->countSubnets(); i++ ) {
        sn = _dhcpsubnetlist->getSubnet(i);
        wanted_ip_ok = wanted_ip_ok ||
            ( ( (req_ip.addr() & sn->_subnet_mask.addr()) != ( sn->_net_address.addr() & sn->_subnet_mask.addr()) ) &&
            ( sn->_vlan == vlanid ) );
        if ( wanted_ip_ok ) {
          req_subnet_mask = sn->_subnet_mask;
          req_net_address = sn->_net_address;
          break;
        }
      }
    }

    if ( ! wanted_ip_ok )
    {
      BRN_DEBUG("BRN2DHCPServer: Client wollte IP: %s ! Diese gehoert nicht zum Subnetz %s/%s",
        req_ip.unparse().c_str(), req_net_address.unparse().c_str(), req_subnet_mask.unparse().c_str());
      BRN_DEBUG("BRN2DHCPServer: Berechne ihm ne neue");

      memcpy(&(client_info->_ciaddr),"\0\0\0\0",4);
      find_client_ip(client_info, vlanid);
    }
    else
    {
      BRN_DEBUG("BRN2DHCPServer: IP ist ok");
      memcpy(&(client_info->_ciaddr),req_ip.data(),4);
    }
  }

  client_info_list.push_back(client_info);

  result = send_dht_request( client_info );

  return( result );
}

int
BRN2DHCPServer::send_dhcp_offer(DHCPClientInfo *client_info )
{
  WritablePacket *p_out = (WritablePacket *)client_info->_client_packet;
  struct dhcp_packet *dhcp_packet_out = (struct dhcp_packet *)p_out->data();
  //memset(dhcp_packet_out, 0, sizeof(struct dhcp_packet));

  uint8_t message_type;
  int result;

  result = DHCPProtocol::set_dhcp_header(p_out, BOOTREPLY);

  dhcp_packet_out->xid = client_info->_xid;
  dhcp_packet_out->secs += 0;
  dhcp_packet_out->flags = htons(BOOTP_BROADCAST);  // set Broadcast-Flag
  memcpy((void*)&dhcp_packet_out->yiaddr,&client_info->_ciaddr,4);
  memcpy((void*)&dhcp_packet_out->chaddr[0],&client_info->_chaddr[0],6);  //set hw-addr

  DHCPProtocol::insertMagicCookie(p_out);
  DHCPProtocol::del_all_options(p_out); //del option of old packet

  message_type = DHCPOFFER;
  DHCPProtocol::add_option(p_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)&message_type);

  uint32_t lease_time;

  lease_time = htonl(_default_lease);
  DHCPProtocol::add_option(p_out,DHO_DHCP_LEASE_TIME ,4 ,(uint8_t *)&lease_time);

  dhcp_add_standard_options(p_out);

  BRN_DEBUG("BRN2DHCPServer: Send DHCP-Offer");

  result = remove_client(client_info);

  output( 0 ).push( p_out ); //erster part
  debug_count_dhcp_packet--;

  return( result );
}

// Client: Anfrage nach angebotenen IP  ( p_in )
// Server: IP bestättigen oder ablehnen ( Packet )

int
BRN2DHCPServer::handle_dhcp_request(Packet *p_in)
{
  int op_size;
  int result;

  DHCPClientInfo *client_info;

  BRN_DEBUG("BRN2DHCPServer: Handle DHCP-Request");

  struct dhcp_packet *new_dhcp_packet = (struct dhcp_packet *)p_in->data();

  client_info = (DHCPClientInfo *)get_client_by_mac(new_dhcp_packet->chaddr);

  if ( client_info != NULL )
  {
    BRN_DEBUG("BRN2DHCPServer: ungeduldiger Client ! Ignoriere erneuten DHCPRequest");

    p_in->kill();
    debug_count_dhcp_packet--;
    return(1);
  }

  BRN_DEBUG("BRN2DHCPServer: Client not in queue! Create new one.");

  client_info = new DHCPClientInfo();

  client_info->_dht_op = MODE_READ_IP;
  client_info->_xid = new_dhcp_packet->xid;
  memcpy(client_info->_chaddr,new_dhcp_packet->chaddr, new_dhcp_packet->hlen );
  client_info->_broadcast = true;
  client_info->_client_packet = p_in;

  int name_size;
  unsigned char *requested_ip = DHCPProtocol::getOption(p_in, DHO_DHCP_REQUESTED_ADDRESS ,&op_size);
  unsigned char *requested_name = DHCPProtocol::getOption(p_in, DHO_HOST_NAME, &name_size);

  if ( ( requested_ip == NULL ) && ( memcmp(&new_dhcp_packet->ciaddr,"\0\0\0\0",4) == 0 ) )
  {
    BRN_ERROR("BRN2DHCPServer: Request ohne IP ??? ERROR?");

    p_in->kill();
    debug_count_dhcp_packet--;
    delete client_info;
    return(1);
  }

  IPAddress req_ip;

  if ( requested_ip != NULL )
  {
    req_ip = IPAddress(requested_ip);

    BRN_DEBUG("IPAddresse is: %s",req_ip.unparse().c_str());

    BRN_DEBUG("BRN2DHCPServer: Clientstatus: SELECTING or INIT-REBOOT");

    unsigned char *server_id = DHCPProtocol::getOption(p_in, DHO_DHCP_SERVER_IDENTIFIER,&op_size);

    if ( server_id == NULL )
    {
      BRN_DEBUG("BRN2DHCPServer: Clientstatus: INIT-REBOOT");

      client_info->_status = DHCPINITREBOOT;
    }
    else
    {

      BRN_DEBUG("BRN2DHCPServer: Clientstatus: SELECTING");

      if ( memcmp(server_id,(uint8_t *)_server_ident.data(),4) != 0 )
      {	
        BRN_DEBUG("DHCP-Server: Client will anderen Server!");

        p_in->kill();
        debug_count_dhcp_packet--;
        delete client_info;
        return(1);
      }

      client_info->_status = DHCPSELECTING;
    }
  }
  else  // requested_ip == NULL
  {
    req_ip = IPAddress(new_dhcp_packet->ciaddr);

    BRN_DEBUG("BRN2DHCPServer: Clientstatus: RENEWING/REBINDING");

    client_info->_status = DHCPRENEWING;
  }

  uint16_t vlanid = getVlanID(EtherAddress(new_dhcp_packet->chaddr));
  IPAddress req_subnet_mask;
  IPAddress req_net_address;

  if ( vlanid == 0 ) {
    req_subnet_mask = _subnet_mask;
    req_net_address = _net_address;
  } else {
    if ( vlanid != 0xFFFF ) {
      BRN2DHCPSubnetList::DHCPSubnet *sn;
      for ( int i = 0; i < _dhcpsubnetlist->countSubnets(); i++ ) {
        sn = _dhcpsubnetlist->getSubnet(i);
        if ( sn->_vlan == vlanid ) {
          req_subnet_mask = sn->_subnet_mask;
          req_net_address = sn->_net_address;
          break;
        }
      }
    } else {
      click_chatter("No vlan for client");
      result = send_dhcp_ack(client_info,DHCPNAK);
      delete client_info;  //löschen weil es nicht zur liste dazukam (TODO)
      return( result );
    }
  }

  if ((req_ip.addr() & req_subnet_mask.addr()) != (req_net_address.addr() & req_subnet_mask.addr()))
  {
    BRN_DEBUG("BRN2DHCPServer: Client wollte IP: %s ! Diese gehoert nicht zum Subnetz %s/%s",
      req_ip.unparse().c_str(), req_net_address.unparse().c_str(), req_subnet_mask.unparse().c_str());

    result = send_dhcp_ack(client_info,DHCPNAK);
    delete client_info;  //löschen weil es nicht zur liste dazukam (TODO)
    return( result );
  }
  else
  {
    BRN_DEBUG("BRN2DHCPServer: IP ist ok");
  }

  memcpy(&client_info->_ciaddr,req_ip.data(),4);

  if ( requested_name != NULL ) {
    client_info->name = String(requested_name);
  } else {
    client_info->name = String(IPAddress(client_info->_ciaddr).unparse().c_str());
  }

  client_info_list.push_back(client_info);
  setSubnet(client_info,getVlanID(EtherAddress(new_dhcp_packet->chaddr)));
  result = send_dht_request(client_info);

  return( result );
}

int
BRN2DHCPServer::send_dhcp_ack(DHCPClientInfo *client_info, uint8_t messagetype)
{
  int result;

  WritablePacket *p_out = (WritablePacket *)client_info->_client_packet;
  struct dhcp_packet *dhcp_packet_out = (struct dhcp_packet *)p_out->data();

  DHCPProtocol::set_dhcp_header(p_out, BOOTREPLY );

  dhcp_packet_out->xid = client_info->_xid;
  dhcp_packet_out->secs += 0;
  dhcp_packet_out->flags = htons(BOOTP_BROADCAST);  // set Broadcast-Flag

  memcpy(dhcp_packet_out->chaddr,client_info->_chaddr,6);  //set hw-addr

  BRN_DEBUG("BRN2DHCPServer: Reqest: set options!");

  DHCPProtocol::insertMagicCookie(p_out);
  DHCPProtocol::del_all_options(p_out);

  DHCPProtocol::add_option(p_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)&messagetype);

  if ( messagetype == DHCPACK )
  {
    memcpy((void*)&dhcp_packet_out->yiaddr,&client_info->_ciaddr,4);

    uint32_t lease_time;

    lease_time = htonl(_default_lease);
    DHCPProtocol::add_option(p_out,DHO_DHCP_LEASE_TIME ,4 ,(uint8_t *)&lease_time);

    dhcp_add_standard_options(p_out);

    if ( _lease_table ) {
      Lease l = Lease();
      l._eth = EtherAddress(client_info->_chaddr);
      l._ip = IPAddress(client_info->_ciaddr);
      l._start = Timestamp::now();
      l._duration = Timestamp(_default_lease);
      l._end = l._start + l._duration;
      l._valid = true;
      _lease_table->insert(l);
    }
  }
  else
  {
    DHCPProtocol::add_option(p_out,DHO_DHCP_SERVER_IDENTIFIER ,4 ,(uint8_t *)_server_ident.data());
  }

  if ( client_info->_broadcast == false )
  {
    dhcp_packet_out->flags = htons(0);  // set Broadcast-Flag
  }

  result = remove_client(client_info);

  BRN_DEBUG("BRN2DHCPServer: Send DHCP-Reply");

  output( 0 ).push( p_out );
  debug_count_dhcp_packet--;

  return( result );

}

int
BRN2DHCPServer::handle_dhcp_release(Packet *p_in)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet *)p_in->data();

  DHCPClientInfo *client_info;

  client_info = (DHCPClientInfo *)get_client_by_mac(dhcp_p->chaddr);

  if ( client_info == NULL )
  {
    BRN_DEBUG("BRN2DHCPServer: Client not in queue! Create new one.");

    client_info = new DHCPClientInfo();
  }
  else
  {
    BRN_DEBUG("BRN2DHCPServer: ungeduldiger Client ! Ignoriere erneuten DHCPRequest");

    p_in->kill();
    return(1);
  }

  if ( _lease_table ) {
    _lease_table->remove(EtherAddress(dhcp_p->chaddr));
  }

  client_info->_status = DHCPBOUND;
  client_info->_dht_op = MODE_REMOVE_IP;
  client_info->_client_packet = p_in;
  memcpy(&client_info->_ciaddr,&dhcp_p->ciaddr,4);
  client_info_list.push_back(client_info);
  setSubnet(client_info,getVlanID(EtherAddress(dhcp_p->chaddr)));
  int result = send_dht_request(client_info);

  return(result);

}

// Client: erhaltene IP ist schon in Benutzung
// Server: sendet neue IP ??

int
BRN2DHCPServer::handle_dhcp_decline(Packet *p_in)
{
  p_in->kill();

  BRN_ERROR("BRN2DHCPServer: Error: Client says IP is used by another Client!");

  return(0);
}


// Client: braucht Konfigurationsarameter, aber keine IP-Lease ( sendet konkrete Wünsche ???)
// Server: Server sendet Konfigurationsarameter ( komplettes dhcp-Packet , was ist mit IP-Feldern ??)

int
BRN2DHCPServer::handle_dhcp_inform(Packet *p_in)
{
  WritablePacket *p = p_in->put(0);

  DHCPProtocol::insertMagicCookie(p);
  DHCPProtocol::del_all_options(p);

  dhcp_add_standard_options(p);

  output( 0 ).push( p );

  return(0);
}

void
BRN2DHCPServer::dhcp_add_standard_options(WritablePacket *p)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet *)p->data();

  memcpy(dhcp_p->sname,_sname.data(),_sname.length());
  dhcp_p->sname[_sname.length()] = '\0';

  DHCPProtocol::add_option(p,DHO_DHCP_SERVER_IDENTIFIER ,4 ,(uint8_t *)_server_ident.data());
  DHCPProtocol::add_option(p,DHO_SUBNET_MASK ,4 ,(uint8_t *)_subnet_mask.data());
  DHCPProtocol::add_option(p,DHO_ROUTERS ,4 ,(uint8_t *)_router.data());
  DHCPProtocol::add_option(p,DHO_DOMAIN_NAME_SERVERS ,4 ,(uint8_t *)_name_server.data());
  DHCPProtocol::add_option(p,DHO_DOMAIN_NAME, _domain_name.length(),(uint8_t *)_domain_name.data());

  return;
}

/* This method returns an ip address to a given ethernet address. */
void
BRN2DHCPServer::find_client_ip(DHCPClientInfo *client_info, uint16_t vlanid)
{
  StringAccum sa;
  uint32_t new_ip;

  BRN_DEBUG("Find ip for Client is Vlan %d", (int)vlanid);
  BRN_DEBUG("id is %d <-> EA is %s",vlanid, EtherAddress(client_info->_chaddr).unparse().c_str());

  if ( _lease_table ) {  //Try to get client ip from local table
    //TODO: handle change of vlan

    Lease *l = _lease_table->rev_lookup(EtherAddress(client_info->_chaddr));
    if ( l != NULL ) {
      new_ip = l->_ip.addr();
    }

  }

  //TODO: check clientIP. Does it fit to subnet of client (vlan)

  if ( memcmp(&(client_info->_ciaddr),"\0\0\0\0",4) == 0 )
  {
    //just take the last byte of mac address --> TODO: ZeroConf

    BRN_DEBUG("BRN2DHCPServer: neue IP");

    setSubnet(client_info, vlanid);

    if ( _dht_storage->range_query_support() ) {
      uint8_t minr[16], maxr[16];  //Try request in local range
      IPRangeQuery iprq = IPRangeQuery(client_info->_subnet_ip, client_info->_subnet_mask);
      _dht_storage->range_query_min_max_id(minr, maxr);
      IPAddress nip = iprq.get_value_for_id(minr);
      new_ip = htonl(ntohl(nip) + (int) client_info->_chaddr[5] + 5);
    } else {
      new_ip = htonl(ntohl(client_info->_subnet_ip.addr()) + (int) client_info->_chaddr[5] + 5);
    }
  }
  else
  {
    memcpy(&new_ip,&(client_info->_ciaddr),4);
    new_ip = htonl(ntohl(new_ip) + 1);
  }

  BRN_DEBUG("BRN2DHCPServer: IP: %s",IPAddress(new_ip).unparse().c_str());
  memcpy(&(client_info->_ciaddr),&new_ip,4);

  return;
}

uint32_t
BRN2DHCPServer::find_lease(void)
{
  return _default_lease;
}

/**************************************************************************************/
/************************** C L I E N T I N F O ***************************************/
/**************************************************************************************/

BRN2DHCPServer::DHCPClientInfo *
BRN2DHCPServer::get_client_by_mac(uint8_t *mac)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( memcmp(client_info_list[i]->_chaddr, mac, 6 ) == 0 ) return( client_info_list[i] );

  return( NULL );
}

BRN2DHCPServer::DHCPClientInfo *
BRN2DHCPServer::get_client_by_dht_id(uint32_t id)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( client_info_list[i]->_id == id ) return( client_info_list[i] );

  return( NULL );

}

int
BRN2DHCPServer::remove_client(DHCPClientInfo *client_info)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( memcmp(client_info_list[i]->_chaddr, client_info->_chaddr, 6 ) == 0 )
    {
      delete client_info_list[i];
      client_info_list.erase(client_info_list.begin() + i);
    }

  return(0);
}

uint16_t
BRN2DHCPServer::getVlanID(EtherAddress ea) {
  if ( _vlantable == NULL ) return 0;
  return _vlantable->find(ea);
}

void
BRN2DHCPServer::setSubnet(DHCPClientInfo *client_info, uint16_t vlanid)
{
  if ( vlanid == 0 || _dhcpsubnetlist == 0 ) {
    client_info->_subnet_ip = _net_address;
    client_info->_subnet_mask = _subnet_mask;
  } else {
    BRN2DHCPSubnetList::DHCPSubnet *sn = _dhcpsubnetlist->getSubnetByVlanID(vlanid);
    if ( sn != NULL ) {
      client_info->_subnet_ip = sn->_net_address;
      client_info->_subnet_mask = sn->_subnet_mask;
    } else {
      BRN_WARN("No Subnet for vlanid. Using default (for vlan 0). Should this be handled in another way.");
      client_info->_subnet_ip = _net_address;
      client_info->_subnet_mask = _subnet_mask;
    }
  }
}

/****************************************************************************/
/************************* H A N D L E R ************************************/
/****************************************************************************/


enum { H_SERVER_INFO };

static String
BRN2DHCPServer_read_param(Element *e, void *thunk)
{
  BRN2DHCPServer *dhcpd = (BRN2DHCPServer *)e;

  switch ((uintptr_t) thunk)
  {
    case H_SERVER_INFO :  return ( dhcpd->server_info( ) );
    default:              return String();
  }
  return String();
}

void
BRN2DHCPServer::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("server_info", BRN2DHCPServer_read_param , (void *)H_SERVER_INFO);
}

String
BRN2DHCPServer::server_info(void)
{
 StringAccum sa;

 sa << "DHCP-Server-INFO\n";
 sa << "Net: " << _net_address.unparse() << "\n";
 sa << "Mask: " <<  _subnet_mask.unparse() << "\n";
 sa << "Broadcast: " << _broadcast_address.unparse() << "\n";

 sa << "Router: " << _router.unparse() << "\n";
 sa << "Server: " << _server_ident << "\n";         //IP DHCP-Server
 sa << "DNS: " << _name_server.unparse() << "\n";        //IP

 sa << "Domain: " << _domain_name << "\n";
 sa << "Queuesize: " << client_info_list.size() << "\n\n";

 sa << "Range: " << start_ip_range.unparse() << "\n\n";

 sa << "DHCP Pcket in Queue: " << debug_count_dhcp_packet << "\n"; 

 return sa.take_string();
}

#include <click/vector.cc>
template class Vector<BRN2DHCPServer::DHCPClientInfo *>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DHCPServer)
