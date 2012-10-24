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
 * restoretunnelendpoint.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "restoretunnelendpoint.hh"

CLICK_DECLS

RestoreTunnelEndpoint::RestoreTunnelEndpoint()
  : _reverse_arp_table(),
    _use_annos(false)
{
  BRNElement::init();
}

RestoreTunnelEndpoint::~RestoreTunnelEndpoint()
{
}

int
RestoreTunnelEndpoint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "REVERSEARPTABLE", cpkP+cpkM, cpElement, &_reverse_arp_table,
      "SETANNO", cpkN, cpBool, &_use_annos,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

int
RestoreTunnelEndpoint::initialize(ErrorHandler *)
{
  return 0;
}

/* learns and stores ip/ethernet address combinations */
Packet *
RestoreTunnelEndpoint::simple_action(Packet *p_in)
{
  BRN_DEBUG("simple_action()");

  click_ip *ip = (click_ip *)(p_in->data());

  const click_ether *ether = (click_ether *)p_in->ether_header();
  EtherAddress dst_ether_addr(ether->ether_dhost);

  IPAddress dst_ip_addr(_reverse_arp_table->lookup(dst_ether_addr));

  if ( _use_annos ) {
    p_in->set_dst_ip_anno(dst_ip_addr);
  } else {
    memcpy(&(ip->ip_dst), dst_ip_addr.data(), 4);
  }

  BRN_DEBUG("* got mapping: %s -> %s", dst_ether_addr.unparse().c_str(), dst_ip_addr.unparse().c_str());

  return p_in;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
RestoreTunnelEndpoint::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RestoreTunnelEndpoint)
