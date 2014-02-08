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
_detection_timer(static_detection_timer_hook, this),
_response_timer(static_response_timer_hook, this),
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

  _detection_timer.initialize(this);
  _response_timer.initialize(this);
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

bool TopologyDetection::path_include_node(uint8_t *path, uint32_t path_len, const EtherAddress *node)
{
  uint8_t *lpath = path;

  for (uint32_t i = 0, pindex = 0; i < path_len; i++, pindex += 6)
    if (memcmp(&(lpath[pindex]), node->data(), 6) == 0) return true;
  return false;
}

void TopologyDetection::start_detection()
{
  dibadawnAlgo.startNewSearch();
}

void TopologyDetection::static_detection_timer_hook(Timer *t, void *f)
{
  if (t == NULL) click_chatter("Time is NULL");
  ((TopologyDetection*) f)->handle_detection_timeout();
}

void TopologyDetection::static_response_timer_hook(Timer *t, void *f)
{
  if (t == NULL) click_chatter("Time is NULL");
  ((TopologyDetection*) f)->handle_response_timeout();
}

void TopologyDetection::handle_detection_timeout(void)
{
  BRN_DEBUG("Detection Timeout. No descendant.");
  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];

  BRN_DEBUG("Finished waiting for response. Now check for bridges.");
  if ((tdi->_src != *(_node_identity->getMasterAddress())) && (tdi->pendant_node()))
    BRN_DEBUG("link between me and parent is bridge ??");

  if ((tdi->_last_hops.size() <= 1) && (tdi->_num_descendant == 0))
    BRN_DEBUG("I'm a lief.");

  if (tdi->_src != *(_node_identity->getMasterAddress()))
    send_response();
  else
    BRN_DEBUG("Source of request. Detection timeout");
}

void TopologyDetection::handle_response_timeout(void)
{
  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];

  if (tdi->_get_backward < tdi->_num_descendant)
    BRN_DEBUG("Missing response from descendant ( %d of %d ).", tdi->_get_backward, tdi->_num_descendant);

  BRN_DEBUG("Finished waiting for response. Now check for bridges.");
  if (tdi->pendant_node())
    BRN_DEBUG("link between me and parent is bridge ??");

  //TODO: the next test failed (looks like that)
  if (tdi->_src != *(_node_identity->getMasterAddress())) //i'm the source of the request. Time to check the result
    send_response(); //send response
  else
    BRN_DEBUG("Source of request. Response timeout");

}

void TopologyDetection::send_response(void)
{
  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];
  TopologyDetection::TopologyDetectionReceivedInfo *tdri = &(tdi->_last_hops[0]);

  BRN_DEBUG("Send Response to %s.", tdri->_addr.unparse().c_str());

  //TODO:ttl
  WritablePacket *p = TopologyDetectionProtocol::new_backwd_packet(&(tdi->_src), tdi->_id, _node_identity->getMasterAddress(), &(tdri->_addr), NULL);
  output(0).push(p);
}

TopologyDetection::TopologyDetectionForwardInfo *
TopologyDetection::get_forward_info(EtherAddress *src, uint32_t id)
{
  for (int i = 0; i < tdfi_list.size(); i++)
  {
    if (tdfi_list[i]->equals(src, id)) return tdfi_list[i];
  }

  return NULL;
}

void
TopologyDetection::evaluate_local_knowledge()
{

}

bool
TopologyDetection::i_am_articulation_point()
{
  return false;
}

void
TopologyDetection::get_bridge_links(Vector<EtherAddress> */*_bridge_links*/)
{
  return;
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

static int write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  TopologyDetection *topo = (TopologyDetection *) e;
  String s = cp_uncomment(in_s);
  switch ((intptr_t) vparam) {
  case H_START_DETECTION:
  { //debug
    int start;
    if (!cp_integer(s, &start))
      return errh->error("start parameter must be Integer");
    topo->start_detection();
    break;
  }
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
