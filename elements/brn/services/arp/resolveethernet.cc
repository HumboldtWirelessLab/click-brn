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
 * ResolveEthernet.{cc,hh} -- restores the ethernet address from an incoming ip-packets and encapsulates packets in Ethernet header
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "resolveethernet.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

ResolveEthernet::ResolveEthernet()
  : _arp_table()
{
  BRNElement::init();
}

ResolveEthernet::~ResolveEthernet()
{
}

int
ResolveEthernet::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
    "ETHERADDRESS", cpkP+cpkM, cpEthernetAddress, &_src,
    "ARPTABLE", cpkP+cpkM, cpElement, &_arp_table,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if (!_arp_table || !_arp_table->cast("ARPTable"))
    return errh->error("ARPTable not specified");

  return 0;
}

/* encapsulates packets in Ethernet header */
Packet *
ResolveEthernet::simple_action(Packet *p_in)
{
  BRN_DEBUG(" simple_action()");

  const click_ip *ip = reinterpret_cast<const click_ip *>(p_in->data());
  IPAddress dst_ip_addr(ip->ip_dst);

  EtherAddress dst_ether_addr(_arp_table->lookup(dst_ip_addr));

  BRN_DEBUG(" * resolved: %s -> %s", dst_ip_addr.unparse().c_str(), dst_ether_addr.unparse().c_str());

  if (WritablePacket *q = p_in->push(14)) {
    click_ether *ether = reinterpret_cast<click_ether *>( q->data());

    memcpy(ether->ether_dhost, dst_ether_addr.data(), 6);
    memcpy(ether->ether_shost, _src.data(), 6);
    ether->ether_type = htons(ETHERTYPE_IP);

    BRN_DEBUG(" * encapsulate in ethernet header: %s | %s | %d", dst_ether_addr.unparse().c_str(), _src.unparse().c_str(), ETHERTYPE_IP);

    // set ether anno
    q->set_ether_header(ether);

    return q;
  }

  return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ResolveEthernet)
