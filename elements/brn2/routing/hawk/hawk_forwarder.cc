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
 * srcforwarder.{cc,hh} -- forwards dsr source routed packets
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "hawk_protocol.hh"
#include "hawk_forwarder.hh"


CLICK_DECLS

HawkForwarder::HawkForwarder()
  : _debug(BrnLogger::DEFAULT),
    _me()
{

}

HawkForwarder::~HawkForwarder()
{
}

int
HawkForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "ROUTINGTABLE", cpkP+cpkM, cpElement, &_rt,
      "FALCONROUTING", cpkP+cpkM, cpElement, &_falconrouting,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
HawkForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
HawkForwarder::uninitialize()
{
  //cleanup
}

void
HawkForwarder::push(int /*port*/, Packet *p_in)
{
  BRN_DEBUG("push(): %d",p_in->length());

  struct hawk_routing_header *header = HawkProtocol::get_route_header(p_in);
  click_ether *ether = HawkProtocol::get_ether_header(p_in);

  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  EtherAddress next(header->_next_etheraddress);

  click_ether *annotated_ether = (click_ether *)p_in->ether_header();
  EtherAddress last_addr(annotated_ether->ether_shost);

  BRN_DEBUG("Me: %s src: %s dst: %s last: %s next: %s",_me->getNodeName().c_str(),
                                                       src_addr.unparse().c_str(),
                                                       dst_addr.unparse().c_str(),
                                                       last_addr.unparse().c_str(),
                                                       next.unparse().c_str());

  /*
   * Here we are sure than the packet comes from the source over last hop,
   * so we add the entry (last hop as next hop to src
   */
  if ( memcmp(src_addr.data(), _falconrouting->_me->_ether_addr.data(), 6) != 0 ) {
    BRN_DEBUG("Node on route, so add info for backward route");
    _rt->addEntry(&(src_addr), header->_src_nodeid, 16, &(last_addr));
  }

  if ( _me->isIdentical(&dst_addr) ) {
    BRN_DEBUG("Is for me");
    HawkProtocol::strip_route_header(p_in);
    output(1).push(p_in);
  } else {
    if ( _me->isIdentical(&next) ) { //i'm next overlay hop
      HawkProtocol::clear_next_hop(p_in);
    }

    EtherAddress _next_dst;

    if ( HawkProtocol::has_next_hop(p_in) )
      _next_dst = next;
    else
      _next_dst = dst_addr;

    BRN_DEBUG("Not for me. Searching for next hop (Dst: %s)",dst_addr.unparse().c_str());

    //check wether i have direct link/path to node
    EtherAddress *next_phy_hop = _rt->getNextHop(&_next_dst);

    if ( next_phy_hop == NULL ) {
      BRN_DEBUG("No next physical hop known. Search for next overlay.");
      //no, so i check for next hop in the overlay

      DHTnode *n = NULL;

      if ( n == NULL ) {
        n = _falconrouting->get_responsibly_node_for_key(header->_dst_nodeid, &(_rt->_known_hosts));
      }

      BRN_DEBUG("Responsible is %s",n->_ether_addr.unparse().c_str());

      if ( n->equalsEtherAddress(_falconrouting->_me) ) { //for clients, which have the
                                                          //same id but different
                                                          // ethereaddress
        HawkProtocol::strip_route_header(p_in);           //TODO: use other output port
                                                          //      for client
        output(1).push(p_in);
        return;
      } else {
        //Since next hop in the overlay is not necessarily my neighbour
        //i use the hawk table to get the real next hop
        next_phy_hop = _rt->getNextHop(&(n->_ether_addr));

        if ( next_phy_hop == NULL ) {
          BRN_ERROR("No next hop for ovelay dst. Kill packet.");
          BRN_ERROR("RT: %s",_rt->routingtable().c_str());
          p_in->kill();
          return;
        }

      }
    }

    BRN_DEBUG("Next hop: %s", next_phy_hop->unparse().c_str());

    if ( _rt->isNeighbour(next_phy_hop) ) {
      BRN_INFO("Nexthop is a neighbour");
      HawkProtocol::set_next_hop(p_in,next_phy_hop);
    } else {
      BRN_INFO("Nexthop is not a neighbour");

      if ( ! HawkProtocol::has_next_hop(p_in) ) HawkProtocol::set_next_hop(p_in,next_phy_hop);

      while ( ( next_phy_hop != NULL ) && (! _rt->isNeighbour(next_phy_hop)) ) {
        next_phy_hop = _rt->getNextHop(next_phy_hop);
      }

      if ( next_phy_hop == NULL ) {
        BRN_ERROR("No valid next hop found. Discard packet.");
        p_in->kill();
        return;
      }
    }

    Packet *brn_p = BRNProtocol::add_brn_header(p_in, BRN_PORT_HAWK, BRN_PORT_HAWK,   255, BRNPacketAnno::tos_anno(p_in));

    BRNPacketAnno::set_src_ether_anno(brn_p,_falconrouting->_me->_ether_addr);  //TODO: take address from anywhere else
    BRNPacketAnno::set_dst_ether_anno(brn_p,*next_phy_hop);
    BRNPacketAnno::set_ethertype_anno(brn_p,ETHERTYPE_BRN);

    output(0).push(brn_p);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  HawkForwarder *sf = (HawkForwarder *)e;
  return String(sf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  HawkForwarder *sf = (HawkForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  sf->_debug = debug;
  return 0;
}

void
HawkForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(HawkForwarder)
