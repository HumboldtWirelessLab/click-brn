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
 * StoreIPEthernet.{cc,hh} -- learns and stores ip/ethernet address combinations from incoming packets.
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "storeipethernet.hh"

CLICK_DECLS

StoreIPEthernet::StoreIPEthernet()
  : _arp_table()
{
  BRNElement::init();
}

StoreIPEthernet::~StoreIPEthernet()
{
}

int
StoreIPEthernet::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ARPTABLE", cpkP+cpkM, cpElement, &_arp_table,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

/* learns and stores ip/ethernet address combinations */
Packet *
StoreIPEthernet::simple_action(Packet *p_in)
{
  BRN_DEBUG("simple_action()");

  const click_ether *ether = reinterpret_cast<const click_ether *>(p_in->data());
  EtherAddress src_ether_addr(ether->ether_shost);

  const click_ip *ip = reinterpret_cast<const click_ip *>((p_in->data() + sizeof(click_ether)));
  IPAddress src_ip_addr(ip->ip_src);

  if (!((src_ip_addr.addr() == 0) || (~src_ip_addr.addr() == 0) || (src_ether_addr.is_broadcast()))) {
    BRN_DEBUG("* new mapping: %s -> %s", src_ether_addr.unparse().c_str(), src_ip_addr.unparse().c_str());
    _arp_table->insert(src_ip_addr, src_ether_addr);
  }

  return p_in;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreIPEthernet)
