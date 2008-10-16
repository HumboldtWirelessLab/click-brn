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

#ifndef ETHERADDREWRITER_HH_
#define ETHERADDREWRITER_HH_
#include <click/element.hh>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <click/etheraddress.hh>
CLICK_DECLS

class SetIPChecksum;
class SetTCPChecksum;
class SetUDPChecksum;

class EtherIpAddrRewriter : public Element 
{
public:
	EtherIpAddrRewriter();
	virtual ~EtherIpAddrRewriter();

public:
  const char *class_name() const  { return "EtherIpAddrRewriter"; }
  const char *port_count() const  { return "1/1"; }
  const char *processing() const  { return PUSH; }

  int configure(Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  //void uninitialize();
  void add_handlers();

  void push(int, Packet *);

  void process_incoming( WritablePacket*, click_ether*, click_ip* );

public:
  bool m_bActive;
  int _debug;

  EtherAddress m_addrEtherSrc;
  EtherAddress m_addrEtherDst;
  IPAddress m_addrIpSrc;
  IPAddress m_addrIpDst;
  
  SetIPChecksum*  m_pIpChecksummer;
  SetTCPChecksum* m_pTcpChecksummer;
  SetUDPChecksum* m_pUdpChecksummer;
};

CLICK_ENDDECLS
#endif /*ETHERADDREWRITER_HH_*/
