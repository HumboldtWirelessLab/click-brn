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
 * BRN2DHCPPrint.{cc,hh} -- responds to dhcp requests
 * A. Zubow
 */

#include <click/config.h>

#include "brn2_dhcpprint.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#define NEED_DHCP_MESSAGE_TYPES
#define NEED_DHCP_MESSAGE_OPTS
#include "dhcp.h"

CLICK_DECLS

BRN2DHCPPrint::BRN2DHCPPrint()
{
}

BRN2DHCPPrint::~BRN2DHCPPrint()
{
}

int
BRN2DHCPPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LABEL", cpkP+cpkM, cpString, /*"label",*/ &_label,
      cpEnd) < 0)
       return -1;
  return 0;
}

int
BRN2DHCPPrint::initialize(ErrorHandler *)
{
  return 0;
}

char *
BRN2DHCPPrint::print_hw_addr (uint8_t, uint8_t hlen, unsigned char *data)
{
  static char habuf [49];
  char *s;
  int i;

  if (hlen <= 0) habuf [0] = 0;
  else
  {
    s = habuf;
    for (i = 0; i < hlen; i++)
    {
      sprintf (s, "%02x", data [i]);
      s += strlen (s);
      *s++ = ':';
    }
    *--s = 0;
  }
  return habuf;
}

/* displays the content of a brn packet */
Packet *
BRN2DHCPPrint::simple_action(Packet *p_in)
{
  click_chatter("BRN2DHCPPrint::simple_action\n");
  click_chatter("****** DHCP packet ***********\n");

  dhcp_packet *dhcp = (dhcp_packet *)p_in->data();

  char opcode[32];
  switch (dhcp->op) {
    case BOOTREQUEST:
      memcpy(opcode, "C->S", strlen("C->S")+1);
      break;
    case BOOTREPLY:
      memcpy(opcode, "S->C", strlen("S->C")+1);
      break;
    default:
      memcpy(opcode, "Unknown", strlen("Unknown")+1);
  }

  click_chatter("* direction (op): %s\n", opcode);

  char htype[32];
  switch (dhcp->htype) {
    case HTYPE_ETHER: {
      String s = "* Ethernet";
      memcpy(htype, s.c_str(), s.length() + 1);
      break;
    }
    case HTYPE_IEEE802: {
      String s = "* Token Ring";
      memcpy(htype, s.c_str(), s.length() + 1);
      break;
    }
    case HTYPE_FDDI: {
      String s = "* FDDI";
      memcpy(htype, s.c_str(), s.length() + 1);
      break;
    }
    default:
      String s = "* Unknown";
      memcpy(htype, s.c_str(), s.length() + 1);
  }

  click_chatter("* Hardware Media type (htype) : %s\n", htype);

  click_chatter("* Hardware address length (in Bytes) (hlen): %d\n", dhcp->hlen);

  click_chatter("* Number of relay agent hops from client (hops): %d\n", dhcp->hops);

  click_chatter("* Transaction ID (XID): 0x%x\n", ntohl(dhcp->xid));

  click_chatter("* Second since client started (secs): %d\n", dhcp->secs);

  click_chatter("* Flags (flags): 0x%x\n", ntohs(dhcp->flags));

  click_chatter("* Client IP address (if already in use) (ciaddr): %s\n", IPAddress(dhcp->ciaddr).unparse().c_str());
  click_chatter("* Client IP address (yiaddr): %s\n", IPAddress(dhcp->yiaddr).unparse().c_str());
  click_chatter("* Next server IP address(siaddr): %s\n", IPAddress(dhcp->siaddr).unparse().c_str());
  click_chatter("* DHCP relay agent IP address(giaddr): %s\n", IPAddress(dhcp->giaddr).unparse().c_str());

  click_chatter("* Client MAC-address (chaddr): %s\n", print_hw_addr(dhcp->htype, dhcp->hlen, dhcp->chaddr));
	
  char servername[DHCP_SNAME_LEN];
  memcpy(servername,dhcp->sname,DHCP_SNAME_LEN);
  servername[DHCP_SNAME_LEN-1] = '\0';
  click_chatter("* DHCP-Servername (sname): %s\n", servername);
	
  if (memcmp(DHCP_OPTIONS_COOKIE, dhcp->options, 4) == 0)
    click_chatter("* Magic Cookie: valid\n");
  else
  {
    click_chatter("* Magic Cookie: invalid\n");
    return 0;
  }

  click_chatter("= Options =\n");

  int code;
  int len;
	char *option_string;

  for (uint32_t offset = 4; offset < ( p_in->length() - DHCP_FIXED_NON_UDP - 1) ;)
  {
    code = dhcp->options[offset];
    len = dhcp->options[offset + 1];

    switch (code) {
      case DHO_PAD:     /* Pad options don't have a length - just skip them. */
          offset++;
          continue;
      case DHO_DHCP_MESSAGE_TYPE:
          if ( dhcp->options[offset+2] > COUNT_OPTIONS ) 
            click_chatter("* dhcp message type: %s\n",message_types[0]);
          else
            click_chatter("* dhcp message type: %s\n",message_types[dhcp->options[offset+2]]);
          break;
      default:
          switch (opt[code].ftype) {
            case ipv4:
                click_chatter("* %s: %d.%d.%d.%d", opt[code].text ,dhcp->options[offset+2], dhcp->options[offset+3], dhcp->options[offset+4], dhcp->options[offset+5]);
                break;
            case ipv4_list:
                click_chatter("* %s: %d.%d.%d.%d", opt[code].text ,dhcp->options[offset+2], dhcp->options[offset+3], dhcp->options[offset+4], dhcp->options[offset+5]);
                break;
            case string:
                option_string = new char [len + 1];
                memcpy(option_string,&dhcp->options[offset+2],len);
                option_string[len] = '\0';
                click_chatter("* %s: %s", opt[code].text,option_string);
                delete[] option_string;
                break;
            case time_in_secs:
                uint32_t time_in_sec;
                memcpy(&time_in_sec,&dhcp->options[offset+2],4);
                time_in_sec = ntohl(time_in_sec);
                click_chatter("* %s: 0x%x%x%x%x = %d seconds",opt[code].text,dhcp->options[offset+2], dhcp->options[offset+3], dhcp->options[offset+4], dhcp->options[offset+5], time_in_sec);
                break;
            default:
                click_chatter("* %s", opt[code].text);
          }
    }
    
    offset = offset + 2 + len;
  }

  click_chatter("******************************\n");

  return p_in;
}


static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2DHCPPrint::add_handlers()
{
  // needed for QuitWatcher
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2DHCPPrint)
