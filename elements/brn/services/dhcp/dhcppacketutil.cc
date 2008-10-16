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
 * dhcppacketutils.{cc,hh} -- dhcp packet utils
 */
 
#include <click/config.h>
#include "elements/brn/common.hh"

#include <click/error.hh>
#include "dhcppacketutil.hh"
CLICK_DECLS

namespace DHCPPacketUtil
{

WritablePacket *new_dhcp_packet(void)
{
  int p_len = DHCP_FIXED_NON_UDP + DHCP_OPTIONS_COOKIE_LEN + 312;
  WritablePacket *new_packet = WritablePacket::make(p_len);
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)new_packet->data(); ;

  memset(new_packet->data(), 0, p_len);

  memcpy( dhcp_p->options, DHCP_OPTIONS_COOKIE, DHCP_OPTIONS_COOKIE_LEN );
  dhcp_p->options[4] = DHO_END;
 
  return(new_packet);
}

int set_dhcp_header(Packet *p, uint8_t _op )
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)p->data(); ;

  dhcp_p->op = _op;
  dhcp_p->htype = HTYPE_ETHER;
  dhcp_p->hlen = 6;
  dhcp_p->hops = 0;                             // have to be zero
  dhcp_p->xid = 0;                              //htonl(c_info->xid);
//  dhcp_p->secs = 0;
  dhcp_p->flags = 0;                             //htons(BOOTP_BROADCAST);  // set Broadcast-Flag
  memcpy((void*)&dhcp_p->ciaddr,"\0\0\0\0",4);
  memcpy((void*)&dhcp_p->yiaddr,"\0\0\0\0",4);
  memcpy((void*)&dhcp_p->siaddr,"\0\0\0\0",4);
  memcpy((void*)&dhcp_p->giaddr,"\0\0\0\0",4);
  
  memcpy(&dhcp_p->chaddr[0],"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",16); //clear hw-addr
  
  dhcp_p->sname[0] = '\0';

  return(0);
    
}


unsigned char *getOptionsField(Packet *p)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)p->data(); ;
  unsigned char *optionsfield = &dhcp_p->options[0];
 
  if ( memcmp(DHCP_OPTIONS_COOKIE, optionsfield, 4 ) != 0)
  {
    click_chatter("DHCPUtils: * Magic Cookie: invalid\n");
    return (NULL);
  }
#ifdef DHCP_PACKETUTIL_DEBUG
  else
    click_chatter("DHCPUtils: * Magic Cookie: valid\n");
#endif

  return ( optionsfield );
}

unsigned char *getOption(Packet *p, int option_num,int *option_size)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)p->data(); ;
//  unsigned char *optionsfield = &dhcp_p->options[0];

  int offset;
	
  offset = 4; // MagicCookie

  while( ( dhcp_p->options[offset] != DHO_END ) && ( offset < (int)( p->length() - DHCP_FIXED_NON_UDP ) ) )
  {
#ifdef DHCP_PACKETUTIL_DEBUG
    click_chatter("DHCPUtils: option_num : %d | curr: %d", option_num, packet->options[offset]);
#endif
    if( dhcp_p->options[offset] == 0 )
    {
      offset++;
      continue;
    }
  
    if( dhcp_p->options[offset] == option_num )
    {
      *option_size = dhcp_p->options[offset + 1];
      return ( &(dhcp_p->options[offset + 2]) )  ;
    }
    offset += ( ( dhcp_p->options[offset + 1] ) + 2 );
  }

  option_size = 0;
  return NULL;
}


void padOptionsField(Packet *p)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)p->data(); ;
//  unsigned char *optionsfield = &dhcp_p->options[0];

  int offset = 4;
  while( dhcp_p->options[offset] != DHO_END )
  {
    if( dhcp_p->options[offset] == 0 )
    {
      offset++;
      continue;
    }
    
    offset += ( ( dhcp_p->options[offset + 1] ) + 2 );
  };

  offset++;

}

void del_all_options(Packet *p)
{
  struct dhcp_packet *packet = (struct dhcp_packet*)p->data(); ;

  int offset = 4;
  packet->options[offset] = DHO_END; //255 after MagicCookie

  offset++;
  while( offset < (int)( p->length() - DHCP_FIXED_NON_UDP ) )
  {
    packet->options[offset] = DHO_PAD;
    offset++;
  }
}

int add_option(Packet *p, int option_num,int option_size,uint8_t *option_val)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)p->data(); ;
//  unsigned char *optionsfield = &dhcp_p->options[0];
  WritablePacket *w_p;

  int add_bytes;

  int offset = 4;
  while ( ( (unsigned int)( DHCP_FIXED_NON_UDP + offset ) < p->length() ) && ( dhcp_p->options[offset] != 0xff ) )
  {
    if( dhcp_p->options[offset] == 0 )
    {
      offset++;
      continue;
    }
    offset += ( ( dhcp_p->options[offset + 1] ) + DHCP_OPTION_OVERHEAD );
  };

  
  if ( dhcp_p->options[offset] != DHO_END )
  {
    click_chatter("DHCPUtils: END not found ! Packet not valid");
    return 1;
  }
	
  add_bytes = ( DHCP_FIXED_NON_UDP + offset + 1 + DHCP_OPTION_OVERHEAD + option_size ) - p->length();

#ifdef DHCP_PACKETUTIL_DEBUG
  click_chatter("DHCPUtils: offset von 255 ist %d",offset);
  click_chatter("DHCPUtils: Packet um %d Bytes grösser machen. Es war %d Bytes gross",add_bytes, p->length());
#endif

  if ( add_bytes > 0 ) w_p = (WritablePacket *)p->put( add_bytes );
  else w_p = (WritablePacket *)p->put( 0 );

  dhcp_p = (struct dhcp_packet*)w_p->data(); ;

  dhcp_p->options[offset] = option_num;
  dhcp_p->options[offset + 1] = option_size;
  memcpy( &(dhcp_p->options[offset + 2]), option_val, option_size);

  dhcp_p->options[offset + 2 + option_size] = DHO_END; //set new end

  return( 0 );
}

void insertMagicCookie(Packet *p)
{
  struct dhcp_packet *dhcp_p = (struct dhcp_packet*)p->data(); ;

  memcpy( dhcp_p->options, DHCP_OPTIONS_COOKIE, 4 );
}

/* Returns the dhcp type of the given packet. In case of a failure -1 is returned. */
int retrieve_dhcptype(Packet *p_in)
{
  uint8_t *dhcp = (uint8_t *)p_in->data();
  uint32_t code, len, offset;

  if ( memcmp(DHCP_OPTIONS_COOKIE, &dhcp[DHCP_FIXED_NON_UDP], 4) != 0)
  {
    click_chatter("DHCPUtils: * Magic Cookie: invalid\n");
    return -1;
  }
#ifdef DHCP_PACKETUTIL_DEBUG
  else
    click_chatter("DHCPUtils: * Magic Cookie: valid\n");
#endif

  for ( offset = DHCP_FIXED_NON_UDP + 4; offset < p_in->length(); ) {
    code = dhcp[offset];
    len = dhcp[offset + 1];

    if (code == DHO_PAD) {
      ++offset;
      continue;
    }

    if (code == DHO_DHCP_MESSAGE_TYPE) {
      return dhcp[offset+2];
    }
    offset = offset + 2 + len;
  }

  return (-1);	
}

}

ELEMENT_PROVIDES(dhcp_packet_util)

CLICK_ENDDECLS
