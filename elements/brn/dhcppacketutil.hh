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

#ifndef DHCPPACKETUTIL_HH
#define DHCPPACKETUTIL_HH

#include "dhcp.h"
#include <click/string.hh>
#include <click/packet.hh>
#include <click/element.hh>

CLICK_DECLS

//#define DHCP_PACKETUTIL_DEBUG

namespace DHCPPacketUtil {

WritablePacket *new_dhcp_packet(void);

int set_dhcp_header(Packet *p, uint8_t _op );

int add_option(Packet *packet, int option_num,int option_size,uint8_t *option_val);
void del_all_options(Packet *p);

unsigned char *getOptionsField(Packet *packet);
unsigned char *getOption(Packet *packet, int option,int *option_size);

void padOptionsField(Packet *packet);

int retrieve_dhcptype(Packet *packet);

void insertMagicCookie(Packet *p);

}
CLICK_ENDDECLS

#endif

