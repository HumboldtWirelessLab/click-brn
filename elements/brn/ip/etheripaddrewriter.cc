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
 * etheripaddrewriter.{cc,hh} - Rewrites ether and ip addresses.
 * Kurth M.
 */

#include <click/config.h>
#include "elements/brn/common.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <elements/ip/setipchecksum.hh>
#include <elements/tcpudp/settcpchecksum.hh>
#include <elements/tcpudp/setudpchecksum.hh>
#include "etheripaddrewriter.hh"
CLICK_DECLS

EtherIpAddrRewriter::EtherIpAddrRewriter() :
  m_bActive( false ),
  _debug(BrnLogger::DEFAULT)
{
  m_pIpChecksummer  = new SetIPChecksum();
  m_pTcpChecksummer = new SetTCPChecksum();
  m_pUdpChecksummer = new SetUDPChecksum();
}

EtherIpAddrRewriter::~EtherIpAddrRewriter()
{
  delete( m_pIpChecksummer );
  delete( m_pTcpChecksummer );
  delete( m_pUdpChecksummer );
}

int 
EtherIpAddrRewriter::configure(Vector<String> & conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
        "DEBUG", cpkP+cpkM, cpInteger, /*"DEBUG",*/ &_debug,
        "ACTIVE", cpkP+cpkM, cpBool, /*"ACTIVE",*/ &m_bActive,
        "ETHER_SRC", cpkP+cpkM, cpEthernetAddress, /*"ETHER_SRC",*/ &m_addrEtherSrc,
        "ETHER_DST", cpkP+cpkM, cpEthernetAddress, /*"ETHER_DST",*/ &m_addrEtherDst,
        "IP_SRC", cpkP+cpkM, cpIPAddress, /*"IP_SRC",*/ &m_addrIpSrc,
        "IP_DST", cpkP+cpkM, cpIPAddress, /*"IP_DST",*/ &m_addrIpDst,
      cpEnd) < 0)
    return -1;

  return( 0 );
}

int 
EtherIpAddrRewriter::initialize(ErrorHandler *)
{
  return( 0 );
}

void 
EtherIpAddrRewriter::push(int port, Packet * p_in)
{
  // If not active, kill all packets.
  if( false == m_bActive )
  {
    p_in->kill();
    return;
  }
  
  // Get the ll and ip headers, kill packet if not set
  WritablePacket* p = p_in->uniqueify();
  click_ether *ether_header = p->ether_header();
  click_ip    *ip_header    = p->ip_header();
  if( NULL == ether_header || NULL == ip_header )
  {
    BRN_ERROR("EtherIpAddrRewriter: header not found, killing packet.");
    p_in->kill();
    return;
  }
  
  switch( port )
  {
  // Input 0: test packets from outside are injected into brn
  case 0:
    process_incoming(p, ether_header, ip_header);
    break;
  // Input 1: test packets injected into brn are separated and send out.
  case 1:
    //process_outgoing(p_in, ether_header, ip_header);
    break;
  default:
    BRN_ERROR("EtherIpAddrRewriter: incoming port not supported, killing packet.\n");
    p_in->kill();
    break;
  };
}

void 
EtherIpAddrRewriter::process_incoming( WritablePacket* p, 
  click_ether* ether_header, click_ip* ip_header )
{
  // A - Ether processing
  // Set new src to old dst (== addr of current node)
  memcpy( ether_header->ether_shost, m_addrEtherSrc.data(), sizeof(ether_header->ether_shost) );
  
  // Set new dst to given dst (== addr of the brn gateway)
  memcpy( ether_header->ether_dhost, m_addrEtherDst.data(), sizeof(ether_header->ether_dhost) );

  
  // B - Ip processing
  // Set new src to given src (== addr of the gateway node)
  ip_header->ip_src = m_addrIpSrc.in_addr();

  // Set new dst to given dst (== addr of the test node outside brn)
  ip_header->ip_dst = m_addrIpDst.in_addr();

  // Correct ip checksum
  Packet* p_out = m_pIpChecksummer->simple_action( p );
  assert(NULL != p_out);
  
  if( IP_PROTO_TCP == ip_header->ip_p )
  {
    p_out = m_pTcpChecksummer->simple_action(p_out);
  }
  else if( IP_PROTO_UDP == ip_header->ip_p )
  {
    p_out = m_pUdpChecksummer->simple_action(p_out);
  }

  // Correct ip checksum and we are out...
  output(0).push( p_out );
}
/*
void 
EtherIpAddrRewriter::process_outgoing(p_in, ether_header, ip_header)
{
  // Do not change anything
  output(0)->push(p_in);
}
*/
static String
read_debug_handler(Element *e, void *)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  return String(rq->_debug) + "\n";
}

static int 
write_debug_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  rq->_debug = debug;
  return 0;
}

static String
read_active_handler(Element *e, void *)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  return String(rq->m_bActive) + "\n";
}

static int 
write_active_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  String s = cp_uncomment(in_s);
  bool active;
  if (!cp_bool(s, &active)) 
    return errh->error("active parameter must be boolean");
  rq->m_bActive = active;
  return 0;
}

static String
read_ether_src_handler(Element *e, void *)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  return rq->m_addrEtherSrc.unparse() + "\n";
}

static int 
write_ether_src_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  String s = cp_uncomment(in_s);
  EtherAddress address;
  if (!cp_ethernet_address(s, &address)) 
    return errh->error("ether_src parameter must be boolean");
  rq->m_addrEtherSrc = address;
  return 0;
}

static String
read_ether_dst_handler(Element *e, void *)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  return rq->m_addrEtherDst.unparse() + "\n";
}

static int 
write_ether_dst_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  String s = cp_uncomment(in_s);
  EtherAddress address;
  if (!cp_ethernet_address(s, &address)) 
    return errh->error("ether_dst parameter must be boolean");
  rq->m_addrEtherDst = address;
  return 0;
}

static String
read_ip_src_handler(Element *e, void *)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  return rq->m_addrIpSrc.unparse() + "\n";
}

static int 
write_ip_src_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  String s = cp_uncomment(in_s);
  IPAddress address;
  if (!cp_ip_address(s, &address)) 
    return errh->error("ip_src parameter must be boolean");
  rq->m_addrIpSrc = address;
  return 0;
}

String
read_ip_dst_handler(Element *e, void *)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  return rq->m_addrIpDst.unparse() + "\n";
}

static int 
write_ip_dst_handler(const String &in_s, Element *e, void *,
          ErrorHandler *errh)
{
  EtherIpAddrRewriter *rq = (EtherIpAddrRewriter *)e;
  String s = cp_uncomment(in_s);
  IPAddress address;
  if (!cp_ip_address(s, &address)) 
    return errh->error("ip_dst parameter must be boolean");
  rq->m_addrIpDst = address;
  return 0;
}

void 
EtherIpAddrRewriter::add_handlers()
{
  add_read_handler(  "debug", read_debug_handler,  0 );
  add_write_handler( "debug", write_debug_handler, 0 );

  add_read_handler(  "active", read_active_handler,  0 );
  add_write_handler( "active", write_active_handler, 0 );

  add_read_handler(  "ether_src", read_ether_src_handler,  0 );
  add_write_handler( "ether_src", write_ether_src_handler, 0 );

  add_read_handler(  "ether_dst", read_ether_dst_handler,  0 );
  add_write_handler( "ether_dst", write_ether_dst_handler, 0 );

  add_read_handler(  "ip_src", read_ip_src_handler,  0 );
  add_write_handler( "ip_src", write_ip_src_handler, 0 );

  add_read_handler(  "ip_dst", read_ip_dst_handler,  0 );
  add_write_handler( "ip_dst", write_ip_dst_handler, 0 );
}

ELEMENT_REQUIRES(SetIPChecksum)
EXPORT_ELEMENT(EtherIpAddrRewriter)
CLICK_ENDDECLS
