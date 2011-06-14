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
 * EewsForwarder.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/brn2.h"

#include "eewsforwarder.hh"
#include "eewsstate.hh"
#include "eewsprotocol.hh"

CLICK_DECLS

EewsForwarder::EewsForwarder():
  alarm_id(0)
{
  BRNElement::init();
}

EewsForwarder::~EewsForwarder()
{
}

int
EewsForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM, cpElement, &_nodeid,
      "EEWSSTATE", cpkP+cpkM, cpElement, &_as,
      "RSSI_DELAY", cpkP, cpBool, &_rssi_delay,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
EewsForwarder::initialize(ErrorHandler *)
{
  click_random_srandom();

  return 0;
}

void
EewsForwarder::push( int port, Packet *p)
{
  uint32_t result;
  struct eews_header *ah = EewsProtocol::getEewsHeader(p);

  if ( port == 0 ) { //from local
    result = _as->update_alarm(ah->type, _nodeid->getMasterAddress(), alarm_id, 0, _nodeid->getMasterAddress());
    WritablePacket *p_n = EewsProtocol::add_node(p, _nodeid->getMasterAddress(), START_TTL, alarm_id);
    alarm_id++;

    WritablePacket *p_out = BRNProtocol::add_brn_header(p_n, BRN_PORT_EEWSPROTOCOL, BRN_PORT_EEWSPROTOCOL, START_TTL, 0);
    BRNPacketAnno::set_ether_anno(p_out, *_nodeid->getMasterAddress(), brn_etheraddress_broadcast, ETHERTYPE_BRN);

    if ( _rssi_delay ) {
      p_out->timestamp_anno() = p->timestamp_anno() + Timestamp::make_msec(click_random() % 5);
    }
    output(0).push(p_out);

  } else if ( port == 1 ) { //from remote
    /*
     * Update alarmstate
     * Mark already forwarded node
    */
    int no_nodes = EewsProtocol::get_count_nodes(p);
    click_ether *ether_header = (click_ether *)p->ether_header();
    EtherAddress fwd_ea = EtherAddress(ether_header->ether_shost);

    for( int i = 0; i < no_nodes; i++ ) {
      struct eews_node *an = EewsProtocol::get_node_struct(p, i);

      EtherAddress ea = EtherAddress(an->node_ea);

      int result = _as->update_alarm(ah->type, &ea, an->id, (an->ttl - BRNPacketAnno::ttl_anno(p)) + 1, &fwd_ea);

      /* if node is to far or already known, mark to delete it */
      if (((an->ttl - BRNPacketAnno::ttl_anno(p)) >= _as->_hop_limit ) ||
          ((result & _as->_forward_flags) == 0 )) {
          an->ttl = DEFAULT_HOP_INVALID;
          BRN_INFO("DELETE INFO");
      }
    }

    /*
     * Remove node, which are to far and already forwarded
     */
    no_nodes = EewsProtocol::remove_node_with_high_ttl(p, BRNPacketAnno::ttl_anno(p) + _as->_hop_limit - 1);

    /*
     *
     */
    if ( no_nodes > 0 ) {  //any node left in the packt ??
//      BRN_ERROR("Forward");
      WritablePacket *p_out = BRNProtocol::add_brn_header(p, BRN_PORT_EEWSPROTOCOL, BRN_PORT_EEWSPROTOCOL,
                                                             BRNPacketAnno::ttl_anno(p)-1, 0);
      BRNPacketAnno::set_ether_anno(p_out, *_nodeid->getMasterAddress(), brn_etheraddress_broadcast, ETHERTYPE_BRN);

      if ( _rssi_delay ) {
        click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
        p_out->timestamp_anno() = p->timestamp_anno() + Timestamp::make_msec((int)ceh->rssi);
      }
      output(0).push(p_out);
    } else {
      p->kill();
    }
  } else {
    p->kill();
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
EewsForwarder::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(EewsForwarder)
