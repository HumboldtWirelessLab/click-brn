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

#include <click/config.h>
#include "common.hh"

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "dhttester.hh"
#include "dhtcommunication.hh"

CLICK_DECLS

DHTTester::DHTTester() : 
  _debug(BrnLogger::DEFAULT),
  request_timer(this)
{

}

DHTTester::~DHTTester()
{
}

int
DHTTester::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String router_ip;
  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpKeywords,
    "DEBUG", cpInteger, "Debug", &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
DHTTester::initialize(ErrorHandler *)
{
  _request_number = 0;

  request_timer.initialize(this);
  request_timer.schedule_after_msec(1000);

  return 0;
}

void
DHTTester::run_timer()
{
//  click_chatter("Timer");
  dht_request();
}

void
DHTTester::push( int port, Packet *packet )  
{
  int result;

  if ( port == 0 )
   result = dht_answer(packet);
}


int
DHTTester::dht_request()
{
  switch ( _request_number)
  {
    case 0:
      {
        click_chatter("DHTTester: ------------- INSERT without sublist ---------------");
      
	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, INSERT, id );

        char *line = "aaaa";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
	char *line_value = "abcdef";
	
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_UNKNOWN, 6, (uint8_t*)line_value );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;
	
	request_timer.schedule_after_msec(1000);
	break;
      }
    case 1:
      {
        click_chatter("DHTTester: ------------- INSERT with sublist ---------------");
	
	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, INSERT, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
	char *line_subkey = "cccc";
	char *line_subvalue = "abcdef";
	
	dht_p_out = DHTPacketUtil::add_sub_list(dht_p_out, 1);
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_UNKNOWN, 4, (uint8_t*)line_subkey );	
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_UNKNOWN, 6, (uint8_t*)line_subvalue );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
      }      
    case 2:
      {
        click_chatter("DHTTester: ------------- INSERT with sublist 2 ---------------");

	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, WRITE, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
	char *line_subkey = "dddd";
	char *line_subvalue = "ghijkl";
	
	dht_p_out = DHTPacketUtil::add_sub_list(dht_p_out, 1);
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_UNKNOWN, 4,(uint8_t*)line_subkey );	
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_UNKNOWN, 6, (uint8_t*)line_subvalue );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
      }      
    case 3:
      {
        click_chatter("DHTTester: ------------- READ with sublist ---------------");
      
	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, READ, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
      }      
    case 4:
      {
       click_chatter("DHTTester: ------------- OVERWRITE with sublist ---------------");
       
	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, WRITE, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
	char *line_subkey = "cccc";
	char *line_subvalue = "gfedcba";
	
	dht_p_out = DHTPacketUtil::add_sub_list(dht_p_out, 1);
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_UNKNOWN, 4,(uint8_t*)line_subkey );	
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 1, TYPE_UNKNOWN, 7, (uint8_t*)line_subvalue );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
	break;
      }      
    case 5:
      {
        click_chatter("DHTTester: ------------- READ with sublist ---------------");

	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, READ, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
      }      
    case 6:
      {
        click_chatter("DHTTester: ------------- REMOVE with sublist ---------------");       
      
	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, REMOVE, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
	char *line_subkey = "dddd";
	
	dht_p_out = DHTPacketUtil::add_sub_list(dht_p_out, 1);
        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_UNKNOWN, 4,(uint8_t*)line_subkey );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
	break;
      }      
    case 7:
      {
        click_chatter("DHTTester: ------------- READ with sublist ---------------");
      
	WritablePacket * dht_p_out = DHTPacketUtil::new_dht_packet();

        uint8_t id = 100;

        DHTPacketUtil::set_header(dht_p_out, DHTTESTER, DHT, 0, READ, id );

        char *line = "bbbb";

        dht_p_out = DHTPacketUtil::add_payload(dht_p_out, 0, TYPE_IP, 4, (uint8_t*)line );	
	
        output( 0 ).push( dht_p_out );
	
	_request_number++;

	request_timer.schedule_after_msec(1000);
	break;
      }      
    }
  return(0);
}

int
DHTTester::dht_answer(Packet *dht_p_in)
{

  dht_p_in->kill();
  return(0);
}




enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  DHTTester *td = (DHTTester *)e;
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
  DHTTester *f = (DHTTester *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void 
DHTTester::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DHTTester)
ELEMENT_REQUIRES(brn_common dhcp_packet_util dhcp_packet_util)
