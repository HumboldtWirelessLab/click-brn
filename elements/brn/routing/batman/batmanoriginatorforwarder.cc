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
 * BatmanOriginatorForwarder.{cc,hh}
 */

#include <click/config.h>
#include "batmanoriginatorforwarder.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/routing/identity/brn2_device.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/routing/batman/batmanprotocol.hh"
#include "elements/brn/brn2.h"


CLICK_DECLS

BatmanOriginatorForwarder::BatmanOriginatorForwarder()
  : _brt(NULL),_nodeid(NULL), _sendbuffer_timer(this)
{
  BRNElement::init();
}

BatmanOriginatorForwarder::~BatmanOriginatorForwarder()
{
}

int
BatmanOriginatorForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM , cpElement, &_nodeid,
      "BATMANTABLE", cpkP+cpkM, cpElement, &_brt,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanOriginatorForwarder::initialize(ErrorHandler *)
{
  click_brn_srandom();
  _sendbuffer_timer.initialize(this);
  return 0;
}

void
BatmanOriginatorForwarder::run_timer(Timer *)
{
  if ( _packet_queue.size() > 0 ) {
    output(0).push(_packet_queue[0]);
    _packet_queue.erase(_packet_queue.begin());
    if ( _packet_queue.size() > 0 )
      _sendbuffer_timer.schedule_after_msec(click_random() % (QUEUE_DELAY));
  }
}

void
BatmanOriginatorForwarder::push( int/*port*/, Packet *packet )
{
  BRN_DEBUG("Push");

  struct batman_header *bh = BatmanProtocol::get_batman_header(packet);
  struct batman_originator *bo = BatmanProtocol::get_batman_originator(packet);

  EtherAddress src = BRNPacketAnno::src_ether_anno(packet);
  uint8_t devId = BRNPacketAnno::devicenumber_anno(packet);
  BRN2Device *dev = _nodeid->getDeviceByIndex(devId);


  BRN_DEBUG("Got originator from %s. Device: %s",src.unparse().c_str(),(*(dev->getEtherAddress())).unparse().c_str() );

  if ( src == EtherAddress(dev->getEtherAddress()->data())) {
    BRN_DEBUG("It's me. Killing.");
    packet->kill();
    return;
  }

  if ( bh->hops == 0 ) { //neighgour
    Timestamp now = Timestamp::now();
    BatmanNeighbourInfo *bni = _neighbour_map.find(src);
    if ( bni == NULL ) {
      _neighbour_map.insert(src,new BatmanNeighbourInfo(100, 2000));
      bni = _neighbour_map.find(src);
    }
    bni->add_originator(ntohl(bo->id), &now);
  }

  uint32_t metric_src_to_me = _brt->get_link_metric(src,*(dev->getEtherAddress()));
  BRN_DEBUG("Metric: originatorsrc <-> me : %d", metric_src_to_me);

  _brt->update_originator(src, src, ntohl(bo->id), 1, 0, metric_src_to_me);

  uint32_t cnt_bn = BatmanProtocol::count_batman_nodes(packet);
  BRN_DEBUG("Found %d nodes in Originator-Message",cnt_bn);

  for( uint32_t bn_i = 0; bn_i < cnt_bn; bn_i++ ) {
    struct batman_node *bn = BatmanProtocol::get_batman_node(packet, bn_i);
    EtherAddress src_ea = EtherAddress(bn->src);

    BRN_DEBUG("Node: %s ID: %d Hops: %d Metric: %d",
              src_ea.unparse().c_str(), ntohl(bn->id), bn->hops + 1, ntohs(bn->metric));

    if ( ! _nodeid->isIdentical(&src_ea) ) { //don't update myself
      _brt->update_originator(src_ea, src, ntohl(bn->id), bn->hops + 1, ntohs(bn->metric), metric_src_to_me);
    }
  }

  if ( false /*newOriginator*/ ) {
    //click_chatter("Originator is new. Forward !");
    bh->hops++;
    WritablePacket *p_fwd = BRNProtocol::add_brn_header(packet, BRN_PORT_BATMAN, BRN_PORT_BATMAN, 10, DEFAULT_TOS);
    BRNPacketAnno::set_ether_anno(p_fwd, dev->getEtherAddress()->data(), brn_ethernet_broadcast, ETHERTYPE_BRN);

    /*Now using queue instead of output(0).push(p_fwd); to reduce collision in simulations*/
    _packet_queue.push_back(p_fwd);
    _sendbuffer_timer.schedule_after_msec(click_random() % ( QUEUE_DELAY ));

  } else {
    //click_chatter("Originator already forward.");
    packet->kill();
  }
}

String
BatmanOriginatorForwarder::print_links()
{
  StringAccum sa;

  sa << "<links node=\"" << _nodeid->getMasterAddress()->unparse() << "\" >\n";

  Timestamp now = Timestamp::now();

  for (BatmanNeighbourInfoMapIter i = _neighbour_map.begin(); i.live();++i) {
    BatmanNeighbourInfo *bni = _neighbour_map.find(i.key());
    uint32_t psr = (uint32_t)(bni->get_fwd_psr(100000,now));
    if ( psr > 0 ) {
      sa << "\t<node addr=\"" << i.key().unparse() << "\" fwd=\"" << psr << "\" />\n";
    }
  }

  sa << "</links>\n";

  return sa.take_string();
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_LINKS
};

static String
read_param(Element *e, void *thunk)
{
  BatmanOriginatorForwarder *bof = reinterpret_cast<BatmanOriginatorForwarder *>(e);

  switch ((uintptr_t) thunk)
  {
    case H_LINKS:
      return ( bof->print_links() );
  }

  return String();
}


void
BatmanOriginatorForwarder::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("links", read_param, H_LINKS);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BatmanProtocol)
EXPORT_ELEMENT(BatmanOriginatorForwarder)
