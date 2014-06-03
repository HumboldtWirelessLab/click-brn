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
 * topology_detection.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>

#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "topology_detection.hh"
#include "topology_detection_protocol.hh"
#include "dibadawn/dibadawn_packet.hh"
#include "dibadawn/nodestatistic.hh"

CLICK_DECLS

TopologyDetection::TopologyDetection() :
detection_id(0),
dibadawnAlgo(this)
{
  BRNElement::init();
}

TopologyDetection::~TopologyDetection()
{
}

int TopologyDetection::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
      "TOPOLOGYINFO", cpkP + cpkM, cpElement, &_topoInfo,
      "NODEIDENTITY", cpkP + cpkM, cpElement, &_node_identity,
      "LINKTABLE", cpkP, cpElement, &_lt,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  dibadawnAlgo.setAddrOfThisNode(*_node_identity->getMasterAddress());
  dibadawnAlgo.setTopologyInfo(_topoInfo);

  return 0;
}

int TopologyDetection::initialize(ErrorHandler *)
{
  click_srandom(_node_identity->getMasterAddress()->hashcode());
  return 0;
}

void TopologyDetection::push(int /*port*/, Packet *packet)
{
  StringAccum sa;
  String node = _node_identity->getMasterAddress()->unparse();
  sa << "\n<topo_detect node=\"" << node << "\">";

  click_ether *ether_h = (click_ether *) packet->ether_header();
  String source = EtherAddress(ether_h->ether_shost).unparse().c_str();
  String destination = EtherAddress(ether_h->ether_dhost).unparse().c_str();
  sa << "\t<etherheader src=\"" << source << "\" des=\"" << destination << "\" />";

  if (memcmp(brn_ethernet_broadcast, ether_h->ether_dhost, 6) == 0)
  {
    handle_detection(packet);
  }
  else
  {
    EtherAddress dst = EtherAddress(ether_h->ether_dhost);
    if (_node_identity->isIdentical(&dst))
    {
      handle_detection(packet);
    }
    else
    {
      sa << "\t<!-- Destination is neither me nor broadcast. TODO: use information (overhear) -->\n";
    }
  }

  sa << "</topo_detect>\n";
  BRN_INFO(sa.take_string().c_str());

  packet->kill();
}

void TopologyDetection::handle_detection(Packet *brn_packet)
{
  DibadawnPacket packet(*brn_packet);
  dibadawnAlgo.receive(packet);
}

void TopologyDetection::start_detection()
{
  dibadawnAlgo.startNewSearch();
}


/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/

/*************************************************************************************************/

String TopologyDetection::local_topology_info(void)
{
  return(_topoInfo->topology_info());
}

enum
{
  H_START_DETECTION, H_TOPOLOGY_INFO
};

static int write_param(const String &click_script_parameter, Element *e, void *vparam, ErrorHandler *errh)
{
  TopologyDetection *topo = (TopologyDetection *) e;
  String s = cp_uncomment(click_script_parameter);
  switch ((intptr_t) vparam) {
  case H_START_DETECTION:
    topo->start_detection();
    break;
  }
  return 0;
}

static String read_param(Element *e, void *thunk)
{
  TopologyDetection *topo = (TopologyDetection *) e;

  switch ((uintptr_t) thunk) {
  case H_TOPOLOGY_INFO:
    return ( topo->local_topology_info());
  default: return String();
  }
}

void TopologyDetection::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("detect", write_param, (void *) H_START_DETECTION);

  add_read_handler("local_topo_info", read_param, (void *) H_TOPOLOGY_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TopologyDetection)
