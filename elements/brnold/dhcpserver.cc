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
#include "common.hh"

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <clicknet/udp.h>
#include <click/error.hh>
#include <click/glue.hh>

#include "dhcpserver.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/string.hh>

#include "dhcppacketutil.hh"
#include "dhtcommunication.hh"

#include "md5.h"

CLICK_DECLS

DHCPServer::DHCPServer() :
  _debug(BrnLogger::DEFAULT)
{
}

DHCPServer::~DHCPServer()
{
  BRN_DEBUG("DHCPServer: end: free all");

  for ( int i = 0; i < client_info_list.size(); i++ )
  {
    client_info_list[i]->_client_packet->kill();
    delete client_info_list[i];
    client_info_list.erase(client_info_list.begin() + i);
  }
}

int
DHCPServer::configure(Vector<String> &conf, ErrorHandler* errh)
{
    BRN_DEBUG("DHCPServer: Configure");

  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpEthernetAddress, "EtherAddress", &_me,
    cpIPPrefix, "address prefix", &_net_address, &_subnet_mask,   /* e.g. "10.9.0.0/16" */
    cpIPAddress, "router", &_router,                  /* e.g. "10.9.0.1" */
    cpIPAddress, "server_identifier", &_server_ident, /* e.g. "10.9.0.1" */
    cpIPAddress, "name server", &_name_server,        /* e.g. "141.20.20.50" */
    cpString, "servername", &_sname,                  /* e.g. "dhcp.brn.net" */
    cpString, "domainname", &_domain_name,            /* e.g. "brn.net" */
    cpKeywords,
    "DEBUG", cpInteger, "Debug", &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
DHCPServer::initialize(ErrorHandler *)
{
  md5_byte_t md5_digest[16];
  md5_state_t state;

  char buf[24];
  
  uint8_t *eth_me = _me.data();

  sprintf(buf, "%02x%02x%02x%02x%02x%02x", eth_me[0], eth_me[1], eth_me[2], eth_me[3], eth_me[4], eth_me[5]);

  MD5::md5_init(&state);
  MD5::md5_append(&state, (const md5_byte_t *)buf, strlen(buf) ); //strlen()
  MD5::md5_finish(&state, md5_digest);

  int mask_shift = _subnet_mask.mask_to_prefix_len();
  int net32 = _net_address.addr();
  net32 = ntohl(net32);
  unsigned int *md5_int_p = (unsigned int*)&md5_digest[0];
  unsigned int md5_int = *md5_int_p;
  md5_int = ntohl(md5_int);
  md5_int = (md5_int >> mask_shift);
  md5_int += net32;
  md5_int = htonl(md5_int);
  start_ip_range = IPAddress(md5_int);

  _default_lease = 3600;
  dht_packet_id = 0;

  debug_count_dhcp_packet = 0;
  debug_count_dht_packet = 0;

//  BRN_DEBUG("Configuration %s: Address space %s/%s, Router %s, ID %s, DNS %s, "
//    "Name %s, Domain %s", _me.s().c_str(), _net_address.s().c_str(), 
//    _subnet_mask.s().c_str(), _router.s().c_str(), _server_ident.s().c_str(),
//    _name_server.s().c_str(), _sname.c_str(), _domain_name.c_str());

  return 0;
}



//Frage von client -> frage an dht -> antwort von dht -> antwort für client

void
DHCPServer::push( int port, Packet *p_in )  
{
  int result = -1;
 
  BRN_DEBUG("DHCPServer: PUSH an Port %d",port);

  if ( port == 0 )
  {
    debug_count_dhcp_packet++;

    BRN_DEBUG("DHCPServer: Responder::dhcpresponse");
	
    int packet_type = DHCPPacketUtil::retrieve_dhcptype(p_in);

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
          BRN_DEBUG("DHCPServer: unknown packet type!!!");
          break;
        }
    }
  }

  if ( port == 1 )
  {
    debug_count_dht_packet++;

    result = handle_dht_answer(p_in);
  }
}

int
DHCPServer::send_dht_question(DHCPClientInfo *client_info)
{
  WritablePacket *dht_p_out;

#ifdef NO_DHT
  struct dht_packet_header *dht_p = NULL;
#endif

  BRN_DEBUG("DHCPServer: Packet for DHT");

  switch ( client_info->_status )
  {
    case DHCPINIT:
    {
	      /* HEADER */
      dht_p_out = DHTPacketUtil::new_dht_packet();

      debug_count_dht_packet++;

      dht_packet_id++;
      client_info->_id = dht_packet_id;
      DHTPacketUtil::set_header(dht_p_out, DHCP, DHT, 0, READ, dht_packet_id );
			
      dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4,(uint8_t*) &client_info->_ciaddr);	

      BRN_DEBUG("DHCPServer: Clientstatus: DHCPINIT");
      BRN_DEBUG("DHCPServer: Send packet to DHT");

      debug_count_dht_packet--;

#ifndef NO_DHT
      output( 1 ).push( dht_p_out );
#else
      dht_p = (struct dht_packet_header *)dht_p_out->data();
      dht_p->flags = KEY_NOT_FOUND;
      handle_dht_answer(dht_p_out);
#endif

      break;
    }
    case DHCPINITREBOOT:
    case DHCPSELECTING:
    case DHCPRENEWING:
    case DHCPREBINDING:
    {
      if ( client_info->_dht_op == READ )
      {
        BRN_DEBUG("DHCPServer: Clientstatus: DHCPSELECTING READ");

        dht_p_out = DHTPacketUtil::new_dht_packet();
        debug_count_dht_packet++;

        dht_packet_id++;
        client_info->_id = dht_packet_id;

        DHTPacketUtil::set_header(dht_p_out, DHCP, DHT, 0, READ, dht_packet_id );
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4,(uint8_t*) &client_info->_ciaddr);	
      }
      else if ( client_info->_dht_op == WRITE )
      {
        BRN_DEBUG("DHCPServer: Clientstatus: DHCPSELECTING WRITE");
        dht_p_out = DHTPacketUtil::new_dht_packet();
        debug_count_dht_packet++;

        dht_packet_id++;
        client_info->_id = dht_packet_id;
					
        DHTPacketUtil::set_header(dht_p_out, DHCP, DHT, 0, WRITE, dht_packet_id );

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4,(uint8_t*) &client_info->_ciaddr);	
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_MAC, 6,(uint8_t*) &client_info->_chaddr);	

        uint32_t lease_time = htonl(_default_lease);
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 2, TYPE_REL_TIME, 4,(uint8_t*)&lease_time );	
      }   
      else
      {
        BRN_ERROR("DHCPServer: send_dht_question: ERROR: UNKNOWN DHT-OPERATION");

        client_info->_client_packet->kill();
        debug_count_dhcp_packet--;
        remove_client(client_info);
        return( -1 );
      }

      BRN_DEBUG("DHCPServer: Send packet to DHT");

#ifndef NO_DHT
      output( 1 ).push( dht_p_out );
#else
      dht_p = (struct dht_packet_header *)dht_p_out->data();
      if ( client_info->_dht_op == READ )
      {
        dht_p->flags = KEY_NOT_FOUND;
      }
      else if ( client_info->_dht_op == WRITE )
      {
        dht_p->flags == 0
      }

      handle_dht_answer(dht_p_out);
#endif

      debug_count_dht_packet--;

      break;
    }	
    default:
      {
        BRN_DEBUG("DHCPServer: send_dht_question: unknown client status !");
        break;
      }
   }

   return(0);
}

int
DHCPServer::handle_dht_answer(Packet *dht_p_in)
{
  int result = -1;

  struct dht_packet_header *dht_p;
  DHCPClientInfo *client_info;

    BRN_DEBUG("DHCPServer: Handle DHT-Answer");

  uint8_t *dht_p_data = (uint8_t*)dht_p_in->data();
  dht_p_data = &dht_p_data[sizeof(struct dht_packet_header)];

  dht_p = (struct dht_packet_header *)dht_p_in->data();

  client_info = (DHCPClientInfo *)get_client_by_dht_id(dht_p->id);
  
  if ( client_info == NULL )
  {
    BRN_DEBUG("DHCPServer: handle_dht_answer: client mit id %d nicht gefunden", dht_p->id );
    dht_p_in->kill();
    debug_count_dht_packet--;
    return( 1 );
  }

  BRN_DEBUG("DHCPServer: Client_status: %d",client_info->_status);

  switch ( client_info->_status )
  {
    case DHCPINIT:
    {
      if ( client_info->_dht_op == READ )
      {
        if ( ( dht_p->flags == KEY_NOT_FOUND ) || ( memcmp(&(dht_p_data[10]), client_info->_chaddr, 6 ) == 0 ) )
        {
          BRN_DEBUG("DHCPServer: IP/Mac is not used or used by the Client !!!");

          result = send_dhcp_offer(client_info);
        }
        else
        {
          BRN_DEBUG("DHCPServer: IP/Mac used by another Client");

          find_client_ip( client_info );
          dht_p_in->kill();
          debug_count_dht_packet--;
          result = send_dht_question(client_info);
          return(result);  //robert
        }
      }
      else
      {
        BRN_ERROR("UNKNOWN DHT-OPERATION FOR DHCPINIT");
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
        case READ:
        {
          if ( ( dht_p->flags == KEY_NOT_FOUND ) || ( memcmp(&(dht_p_data[10]),client_info->_chaddr,6 ) == 0 ) )
          {
             BRN_DEBUG("DHCPServer: IP is not used or used by this client. Write new MAC to DHT!");

             client_info->_dht_op = WRITE;
             dht_p_in->kill();
             debug_count_dht_packet--;
             result = send_dht_question(client_info);
             return(result);
          }
          else
          {
            BRN_DEBUG("DHCPServer: IP is used by another Client");
            result = send_dhcp_ack(client_info,DHCPNAK );
          }
          break;
        }
        case WRITE:
        {
          BRN_DEBUG("DHCPServer: Write IP to DHT");
          if ( dht_p->flags == 0 ) //all O.K.
          {
            result = send_dhcp_ack(client_info, DHCPACK );
          }
          else
          {
            BRN_ERROR("DHCPServer: ERROR: COULDN'T WRITE TO DHT");
            result = send_dhcp_ack(client_info, DHCPNAK );
          }
          break;
        }
        default:
        {
          BRN_ERROR("DHCPServer: ERROR: UNKNOWN DHT-OPERATION");
          result = 1;
          break;
        }
      }
      break;
    }
    case DHCPBOUND:
    {
      if ( client_info->_dht_op == REMOVE )
      {
        BRN_DEBUG("DHCPServer: Client wurde entfernt");
        client_info->_client_packet->kill();
        debug_count_dhcp_packet--;
        result = remove_client(client_info);
      }
      break;
    }
    default:
    {
      BRN_DEBUG("DHCPServer: dht-answer unknown client status !");
      result = 1;
      break;
    }
  }
  
  dht_p_in->kill();
  debug_count_dht_packet--;
	
  return(result);
}

// Client: DHCP-Server suchen   ( p_in )
// Server: IP-Anbieten          ( Packet )

int
DHCPServer::handle_dhcp_discover(Packet *p_in)
{
  BRN_DEBUG("DHCPServer: Clientstatus: INIT!");
  BRN_DEBUG("DHCPServer: Handle DHCP-Discover");

  int result;

  DHCPClientInfo *client_info;

  struct dhcp_packet *new_dhcp_packet = (struct dhcp_packet *)p_in->data();

  client_info = (DHCPClientInfo *)get_client_by_mac(new_dhcp_packet->chaddr);

  if ( client_info == NULL )
  {
    BRN_DEBUG("DHCPServer: Client not in queue! Create new one.");

    client_info = new DHCPClientInfo();
  }
  else
  {
    BRN_DEBUG("DHCPServer: ungeduldiger Client ! Ignoriere erneutes DHCPDiscover");

    p_in->kill();
    debug_count_dhcp_packet--;
    return(0);
  }
  
  client_info->_status = DHCPINIT;
  client_info->_dht_op = READ;
  client_info->_broadcast = true;
  client_info->_xid = new_dhcp_packet->xid;
  memcpy(client_info->_chaddr,new_dhcp_packet->chaddr,6);

  client_info->_client_packet = p_in;

  int op_size;
  unsigned char *requested_ip = DHCPPacketUtil::getOption(p_in, DHO_DHCP_REQUESTED_ADDRESS ,&op_size);

  if ( ( requested_ip == NULL ) && ( memcmp(&new_dhcp_packet->ciaddr,"\0\0\0\0",4) == 0 ) )
  {
    memcpy(&(client_info->_ciaddr),"\0\0\0\0",4);
    find_client_ip(client_info);
  }     
  else
  {
    BRN_DEBUG("DHCPServer: INIT. Client want special IP");

    IPAddress req_ip;

    if ( requested_ip != NULL )
      req_ip = IPAddress(requested_ip);
    else
      req_ip = IPAddress(new_dhcp_packet->ciaddr);
   
    if ((req_ip.addr() & _subnet_mask.addr()) != (_net_address.addr() & _subnet_mask.addr()))
    {
      BRN_DEBUG("DHCPServer: Client wollte IP: %s ! Diese gehoert nicht zum Subnetz %s/%s",
        req_ip.s().c_str(), _net_address.s().c_str(), _subnet_mask.s().c_str());
      BRN_DEBUG("DHCPServer: Berechne ihm ne neue");
	
      memcpy(&(client_info->_ciaddr),"\0\0\0\0",4);
      find_client_ip(client_info);
    }
    else
    {
      BRN_DEBUG("DHCPServer: IP ist ok");
      memcpy(&(client_info->_ciaddr),req_ip.data(),4);
    }
  }

  client_info_list.push_back(client_info);

  result = send_dht_question( client_info );

  return( result );
}


int
DHCPServer::send_dhcp_offer(DHCPClientInfo *client_info )
{
  WritablePacket *p_out = (WritablePacket *)client_info->_client_packet;
  struct dhcp_packet *dhcp_packet_out = (struct dhcp_packet *)p_out->data();
  //memset(dhcp_packet_out, 0, sizeof(struct dhcp_packet));

  uint8_t message_type;
  int result;

  result = DHCPPacketUtil::set_dhcp_header(p_out, BOOTREPLY);

  dhcp_packet_out->xid = client_info->_xid;
  dhcp_packet_out->secs += 0;
  dhcp_packet_out->flags = htons(BOOTP_BROADCAST);  // set Broadcast-Flag
  memcpy((void*)&dhcp_packet_out->yiaddr,&client_info->_ciaddr,4);
  memcpy((void*)&dhcp_packet_out->chaddr[0],&client_info->_chaddr[0],6);  //set hw-addr

  DHCPPacketUtil::insertMagicCookie(p_out);
  DHCPPacketUtil::del_all_options(p_out); //del option of old packet
	
  message_type = DHCPOFFER;
  DHCPPacketUtil::add_option(p_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)&message_type);

  uint32_t lease_time;

  lease_time = htonl(_default_lease);
  DHCPPacketUtil::add_option(p_out,DHO_DHCP_LEASE_TIME ,4 ,(uint8_t *)&lease_time);
 
  dhcp_add_standard_options(p_out);
	
  BRN_DEBUG("DHCPServer: Send DHCP-Offer");

  result = remove_client(client_info);

  output( 0 ).push( p_out ); //erster part
  debug_count_dhcp_packet--;

  return( result );
}

// Client: Anfrage nach angebotenen IP  ( p_in )
// Server: IP bestättigen oder ablehnen ( Packet )

int
DHCPServer::handle_dhcp_request(Packet *p_in)
{
  int op_size;
  int result;

  DHCPClientInfo *client_info;

    BRN_DEBUG("DHCPServer: Handle DHCP-Request");

  struct dhcp_packet *new_dhcp_packet = (struct dhcp_packet *)p_in->data();

  client_info = (DHCPClientInfo *)get_client_by_mac(new_dhcp_packet->chaddr);

  if ( client_info != NULL )
  {
    BRN_DEBUG("DHCPServer: ungeduldiger Client ! Ignoriere erneuten DHCPRequest");

    p_in->kill();
    debug_count_dhcp_packet--;
    return(1);
  }

  BRN_DEBUG("DHCPServer: Client not in queue! Create new one.");

  client_info = new DHCPClientInfo();

  client_info->_dht_op = READ;
  client_info->_xid = new_dhcp_packet->xid;
  memcpy(client_info->_chaddr,new_dhcp_packet->chaddr, new_dhcp_packet->hlen );
  client_info->_broadcast = true;
  client_info->_client_packet = p_in;

  unsigned char *requested_ip = DHCPPacketUtil::getOption(p_in, DHO_DHCP_REQUESTED_ADDRESS ,&op_size);

  if ( ( requested_ip == NULL ) && ( memcmp(&new_dhcp_packet->ciaddr,"\0\0\0\0",4) == 0 ) )
  {
    BRN_ERROR("DHCPServer: Request ohne IP ??? ERROR?");

    p_in->kill();
    debug_count_dhcp_packet--;
    delete client_info;
    return(1);
  }

  IPAddress req_ip;

  if ( requested_ip != NULL )
  {
    req_ip = IPAddress(requested_ip);

    BRN_DEBUG("DHCPServer: Clientstatus: SELECTING or INIT-REBOOT");

    unsigned char *server_id = DHCPPacketUtil::getOption(p_in, DHO_DHCP_SERVER_IDENTIFIER,&op_size);
			
    if ( server_id == NULL )
    {
      BRN_DEBUG("DHCPServer: Clientstatus: INIT-REBOOT");

      client_info->_status = DHCPINITREBOOT;
    }
    else  
    {

      BRN_DEBUG("DHCPServer: Clientstatus: SELECTING");

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

    BRN_DEBUG("DHCPServer: Clientstatus: RENEWING/REBINDING");

    client_info->_status = DHCPRENEWING;
  }

   
  if ((req_ip.addr() & _subnet_mask.addr()) != (_net_address.addr() & _subnet_mask.addr()))
  {
    BRN_DEBUG("DHCPServer: Client wollte IP: %s ! Diese gehoert nicht zum Subnetz %s/%s",
      req_ip.s().c_str(), _net_address.s().c_str(), _subnet_mask.s().c_str());

    result = send_dhcp_ack(client_info,DHCPNAK);
    delete client_info;  //löschen weil es nicht zur liste dazukam (TODO)
    return( result );
  }
  else
  {
    BRN_DEBUG("DHCPServer: IP ist ok");
  }

  memcpy(&client_info->_ciaddr,req_ip.data(),4);

  client_info_list.push_back(client_info);

  result = send_dht_question(client_info);

  return( result );
}


int
DHCPServer::send_dhcp_ack(DHCPClientInfo *client_info, uint8_t messagetype)
{ 
  int result;
  
  WritablePacket *p_out = (WritablePacket *)client_info->_client_packet;
  struct dhcp_packet *dhcp_packet_out = (struct dhcp_packet *)p_out->data();

  DHCPPacketUtil::set_dhcp_header(p_out, BOOTREPLY );

  dhcp_packet_out->xid = client_info->_xid;
  dhcp_packet_out->secs += 0;
  dhcp_packet_out->flags = htons(BOOTP_BROADCAST);  // set Broadcast-Flag
  
  memcpy(dhcp_packet_out->chaddr,client_info->_chaddr,6);  //set hw-addr

  BRN_DEBUG("DHCPServer: Reqest: set options!");

  DHCPPacketUtil::insertMagicCookie(p_out);
  DHCPPacketUtil::del_all_options(p_out);
		
  DHCPPacketUtil::add_option(p_out,DHO_DHCP_MESSAGE_TYPE,1,(uint8_t *)&messagetype);
  
  if ( messagetype == DHCPACK )
  {
    memcpy((void*)&dhcp_packet_out->yiaddr,&client_info->_ciaddr,4);
 
    uint32_t lease_time;

    lease_time = htonl(_default_lease);
    DHCPPacketUtil::add_option(p_out,DHO_DHCP_LEASE_TIME ,4 ,(uint8_t *)&lease_time);

    dhcp_add_standard_options(p_out);
  }
  else
  {
    IPAddress server_ident;
    cp_ip_address(_server_ident, &server_ident, this);
    DHCPPacketUtil::add_option(p_out,DHO_DHCP_SERVER_IDENTIFIER ,4 ,(uint8_t *)server_ident.data());
  }

  if ( client_info->_broadcast == false )
  {
    dhcp_packet_out->flags = htons(0);  // set Broadcast-Flag
  }
  
  result = remove_client(client_info);

  BRN_DEBUG("DHCPServer: Send DHCP-Reply");

  output( 0 ).push( p_out );
  debug_count_dhcp_packet--;

  return( result );

}



int
DHCPServer::handle_dhcp_release(Packet *p_in)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet *)p_in->data();
  
  DHCPClientInfo *client_info;

  client_info = (DHCPClientInfo *)get_client_by_mac(dhcp_p->chaddr);

  if ( client_info == NULL )
  {
    BRN_DEBUG("DHCPServer: Client not in queue! Create new one.");

    client_info = new DHCPClientInfo();
  }
  else
  {
    BRN_DEBUG("DHCPServer: ungeduldiger Client ! Ignoriere erneuten DHCPRequest");

    p_in->kill();
    return(1);
  }

  client_info->_status = DHCPBOUND;
  client_info->_dht_op = REMOVE;
  dht_packet_id++;
  client_info->_id = dht_packet_id;
  client_info->_client_packet = p_in;

  memcpy(&client_info->_ciaddr,&dhcp_p->ciaddr,4);

  WritablePacket *dht_p_out = DHTPacketUtil::new_dht_packet();
  debug_count_dht_packet++;


  DHTPacketUtil::set_header(dht_p_out, DHCP, DHT, 0, REMOVE, dht_packet_id );

  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP , 4,(uint8_t*) &dhcp_p->yiaddr);	
  dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_MAC, 6,(uint8_t*) &dhcp_p->chaddr);	

  client_info_list.push_back(client_info);

#ifndef NO_DHT
  output( 1 ).push( dht_p_out );
#else
  handle_dht_answer(dht_p_out);
#endif

  debug_count_dht_packet--;
 
  return(0);

}

// Client: erhaltene IP ist schon in Benutzung
// Server: sendet neue IP ??

int
DHCPServer::handle_dhcp_decline(Packet *p_in)
{
  p_in->kill();

  BRN_ERROR("DHCPServer: Error: Client says IP is used by another Client!");

  return(0);
}


// Client: braucht Konfigurationsarameter, aber keine IP-Lease ( sendet konkrete Wünsche ???)
// Server: Server sendet Konfigurationsarameter ( komplettes dhcp-Packet , was ist mit IP-Feldern ??)

int
DHCPServer::handle_dhcp_inform(Packet *p_in)
{
  WritablePacket *p = p_in->put(0);

  DHCPPacketUtil::insertMagicCookie(p);
  DHCPPacketUtil::del_all_options(p);

  dhcp_add_standard_options(p);

  output( 0 ).push( p );

  return(0);
}



void
DHCPServer::dhcp_add_standard_options(WritablePacket *p)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet *)p->data();
  
  memcpy(dhcp_p->sname,_sname.data(),_sname.length());
  dhcp_p->sname[_sname.length()] = '\0';

  IPAddress server_ident;
  cp_ip_address(_server_ident, &server_ident, this);
  DHCPPacketUtil::add_option(p,DHO_DHCP_SERVER_IDENTIFIER ,4 ,(uint8_t *)server_ident.data());
			
  IPAddress subnet_mask;
  cp_ip_address(_subnet_mask, &subnet_mask, this);
  DHCPPacketUtil::add_option(p,DHO_SUBNET_MASK ,4 ,(uint8_t *)subnet_mask.data());

  IPAddress router;
  cp_ip_address(_router, &router, this);
  DHCPPacketUtil::add_option(p,DHO_ROUTERS ,4 ,(uint8_t *)router.data());

  IPAddress name_server;
  cp_ip_address(_name_server, &name_server, this);
  DHCPPacketUtil::add_option(p,DHO_DOMAIN_NAME_SERVERS ,4 ,(uint8_t *)name_server.data());

  DHCPPacketUtil::add_option(p,DHO_DOMAIN_NAME, _domain_name.length(),(uint8_t *)_domain_name.data());

  return;
}

/* This method returns an ip address to a given ethernet address. */
void
DHCPServer::find_client_ip(DHCPClientInfo *client_info)
{
  StringAccum sa;
  uint32_t new_ip;
 
  if ( memcmp(&(client_info->_ciaddr),"\0\0\0\0",4) == 0 )
  {
//just take the last byte of mac address --> TODO: ZeroConf

    BRN_DEBUG("DHCPServer: neue IP");
     
    new_ip = ntohl(_net_address.addr());
    new_ip = new_ip + (int) client_info->_chaddr[5] + 5;
    new_ip = htonl(new_ip);

    IPAddress _tmp_ip(new_ip);

    BRN_DEBUG("DHCPServer: IP: %s",_tmp_ip.s().c_str()); 

    memcpy(&(client_info->_ciaddr),&new_ip,4);
  }
  else
  {
    memcpy(&new_ip,&(client_info->_ciaddr),4);
    new_ip = ntohl(new_ip);

    new_ip += 1;
    
    new_ip = htonl(new_ip);
    memcpy(&(client_info->_ciaddr),&new_ip,4);
  }

  return;
}

uint32_t
DHCPServer::find_lease(void)
{
  return _default_lease;
}


void *
DHCPServer::get_client_by_mac(uint8_t *mac)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( memcmp(client_info_list[i]->_chaddr, mac, 6 ) == 0 ) return( client_info_list[i] );
 
  return( NULL );
}

void *
DHCPServer::get_client_by_dht_id(uint8_t id)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( client_info_list[i]->_id == id ) return( client_info_list[i] );
 
  return( NULL );

}

int
DHCPServer::remove_client(DHCPClientInfo *client_info)
{
  for ( int i = 0; i < client_info_list.size(); i++ )
    if ( memcmp(client_info_list[i]->_chaddr, client_info->_chaddr, 6 ) == 0 )
    {
      delete client_info_list[i];
      client_info_list.erase(client_info_list.begin() + i);
    }
   
  return(0);
}




enum { H_SERVER_INFO, H_DEBUG };

static String 
read_param(Element *e, void *thunk)
{
  DHCPServer *td = (DHCPServer *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  DHCPServer *f = (DHCPServer *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }}
  return 0;
}

static String                                                                                                                                                          
DHCPServer_read_param(Element *e, void *thunk)
{
  DHCPServer *dhcpd = (DHCPServer *)e;
  
  switch ((uintptr_t) thunk)
  {
    case H_SERVER_INFO :  return ( dhcpd->server_info( ) );
    default:              return String();
  }
  return String();
}

void
DHCPServer::add_handlers()
{
  add_read_handler("server_info", DHCPServer_read_param , (void *)H_SERVER_INFO);

  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

String
DHCPServer::server_info(void)
{
 StringAccum sa;
 
 sa << "DHCP-Server-INFO\n";
 sa << "Net: " << _net_address.s() << "\n";
 sa << "Mask: " <<  _subnet_mask.s() << "\n";
 sa << "Broadcast: " << _broadcast_address.s() << "\n";

 sa << "Router: " << _router.s() << "\n";
 sa << "Server: " << _server_ident << "\n";         //IP DHCP-Server
 sa << "DNS: " << _name_server.s() << "\n";        //IP

 sa << "Domain: " << _domain_name << "\n";
 sa << "Queuesize: " << client_info_list.size() << "\n\n";

 sa << "Range: " << start_ip_range.s() << "\n\n";
 
 sa << "DHT Packet in Queue: " << debug_count_dht_packet << "\n"; 
 sa << "DHCP Pcket in Queue: " << debug_count_dhcp_packet << "\n"; 

 return sa.take_string();
}


#include <click/vector.cc>
template class Vector<DHCPServer::DHCPClientInfo *>;

CLICK_ENDDECLS
ELEMENT_REQUIRES(brn_common dhcp_packet_util dhcp_packet_util)
EXPORT_ELEMENT(DHCPServer)
