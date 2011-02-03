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

CLICK_DECLS

TopologyDetection::TopologyDetection() :
  _detection_timer(static_detection_timer_hook,this),
  _response_timer(static_response_timer_hook,this),
  detection_id(0)
{
  BRNElement::init();
}

TopologyDetection::~TopologyDetection()
{
}

int
TopologyDetection::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "TOPOLOGYINFO", cpkP+cpkM, cpElement, &_topoi,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_identity,
      "LINKTABLE", cpkP, cpElement, &_lt,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
TopologyDetection::initialize(ErrorHandler *)
{
  click_srandom(_node_identity->getMasterAddress()->hashcode());

  _detection_timer.initialize(this);
  _response_timer.initialize(this);
  return 0;
}


void
TopologyDetection::push( int /*port*/, Packet *packet )
{
  click_ether *ether_h = (click_ether *)packet->ether_header();

  BRN_DEBUG("Etherheader: Src: %s Dst: %s", EtherAddress(ether_h->ether_shost).unparse().c_str(),
                                            EtherAddress(ether_h->ether_dhost).unparse().c_str());

  if ( memcmp(brn_ethernet_broadcast, ether_h->ether_dhost, 6) == 0 ) {
    handle_detection_forward(packet);
  } else {
    EtherAddress dst = EtherAddress(ether_h->ether_dhost);
    if ( _node_identity->isIdentical(&dst) ) {
      handle_detection_backward(packet);
    } else {
      BRN_DEBUG("Destination is neither me nor broadcast. TODO: use information (overhear).");
      packet->kill();
    }
  }
}

void
TopologyDetection::handle_detection_forward(Packet *packet)
{
  uint32_t id;
  uint8_t entries;
  uint8_t ttl;
  uint8_t *path;
  EtherAddress src;
  click_ether *ether_h = (click_ether *)packet->ether_header();
  bool new_td = false;

  path = TopologyDetectionProtocol::get_info(packet, &src, &id, &entries, &ttl);

  TopologyDetectionForwardInfo *tdi = get_forward_info(&src, id);

  if ( tdi == NULL ) {
    BRN_DEBUG("Unknown topology detection request. Src: %s Id: %d Entries: %d TTL: %d.",src.unparse().c_str(),id,entries,ttl);
    tdfi_list.push_back(new TopologyDetectionForwardInfo(&src, id, (ttl-1)));
    tdi = get_forward_info(&src, id);
    new_td = true;
  }

  EtherAddress last_hop = EtherAddress(ether_h->ether_shost);

  bool incl_me = path_include_node(path, entries, _node_identity->getMasterAddress());

  tdi->add_last_hop(&last_hop,ttl,incl_me);

  if ( new_td ) {
    BRN_DEBUG("New request. Forward the request.");
    WritablePacket *fwd_p = TopologyDetectionProtocol::fwd_packet(packet, _node_identity->getMasterAddress(), &last_hop);

    _detection_timer.schedule_after_msec( /*click_random() %*/ /*100*/ ttl * ttl / 10 );  //wait for forward of an descendant

    output(0).push(fwd_p);

  } else {
    BRN_DEBUG("I know the request. Check state and kill the packet.");

    if ( (( tdi->_ttl - 1 ) == ttl) && incl_me ) { //is my descendants (nachkomme)
      BRN_DEBUG("It's my descendants.");
      tdi->set_descendant(&last_hop,true);
      tdi->_num_descendant++;
      _detection_timer.unschedule();
      _response_timer.unschedule();
      _response_timer.schedule_after_msec( /*click_random() %*/ (tdi->_ttl * tdi->_ttl) );
    }

    packet->kill();
  }
}

bool
TopologyDetection::path_include_node(uint8_t *path, uint32_t path_len, const EtherAddress *node )
{
  uint8_t *lpath = path;

  for ( uint32_t i = 0,pindex = 0; i < path_len; i++, pindex+=6)
    if ( memcmp(&(lpath[pindex]), node->data(), 6 ) == 0 ) return true;
  return false;
}

void
TopologyDetection::handle_detection_backward(Packet *packet)
{
  BRN_DEBUG("Handle backward");

  click_ether *ether_h = (click_ether *)packet->ether_header();

  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];
  tdi->_get_backward++;

  BRN_DEBUG("Src: %s Back: %d of %d",EtherAddress(ether_h->ether_shost).unparse().c_str(),tdi->_get_backward, tdi->_num_descendant);

  EtherAddress last_hop = EtherAddress(ether_h->ether_shost);
  //TODO: does a backward packet includes a path ?????
  if ( ! tdi->include_last_hop(&last_hop) ) {
    EtherAddress src;
    uint32_t id;
    uint8_t entries;
    uint8_t ttl;
    uint8_t *path;

    path = TopologyDetectionProtocol::get_info(packet, &src, &id, &entries, &ttl);

    tdi->add_last_hop(&last_hop, ttl, true);
    tdi->set_descendant(&last_hop,true);
    tdi->_num_descendant++;
  }

  if ( tdi->_get_backward == tdi->_num_descendant )
  {
    BRN_DEBUG("Got infos off all descendant");
    _response_timer.unschedule();
    _response_timer.schedule_after_msec( /*click_random() % */100 );  //wait only small time for additional response
  }

  if ( tdi->_get_backward > tdi->_num_descendant )
  {
    BRN_ERROR("Got more back than wanted");
    _response_timer.unschedule();
    _response_timer.schedule_after_msec( /*click_random() % */100 );  //wait only small time for additional response
  }
}

void
TopologyDetection::start_detection()
{
  detection_id++;
  WritablePacket *p = TopologyDetectionProtocol::new_detection_packet(_node_identity->getMasterAddress(), detection_id, 128);

  tdfi_list.push_back(new TopologyDetectionForwardInfo(_node_identity->getMasterAddress(), detection_id, 128));
  _detection_timer.schedule_after_msec( /*click_random() % */128*12 );  //wait for descendant

  output(0).push(p);

}

void
TopologyDetection::static_detection_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((TopologyDetection*)f)->handle_detection_timeout();
}

void
TopologyDetection::static_response_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((TopologyDetection*)f)->handle_response_timeout();
}

void
TopologyDetection::handle_detection_timeout(void)
{
  BRN_DEBUG("Detection Timeout. No descendant.");
  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];

  BRN_DEBUG("Finished waiting for response. Now check for bridges.");
  if ( (tdi->_src != *(_node_identity->getMasterAddress())) && (tdi->pendant_node()) )
    BRN_DEBUG("link between me and parent is bridge ??");

  if ( (tdi->_last_hops.size() <= 1) && (tdi->_num_descendant == 0) )
    BRN_DEBUG("I'm a lief.");

  if ( tdi->_src != *(_node_identity->getMasterAddress()) )
    send_response();
  else
    BRN_DEBUG("Source of request. Detection timeout");
}

void
TopologyDetection::handle_response_timeout(void)
{
  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];

  if ( tdi->_get_backward < tdi->_num_descendant )
    BRN_DEBUG("Missing response from descendant ( %d of %d ).",tdi->_get_backward, tdi->_num_descendant);

  BRN_DEBUG("Finished waiting for response. Now check for bridges.");
  if ( tdi->pendant_node() )
    BRN_DEBUG("link between me and parent is bridge ??");

//TODO: the next test failed (looks like that)
  if ( tdi->_src != *(_node_identity->getMasterAddress()) ) //i'm the source of the request. Time to check the result
    send_response();                                        //send response
  else
    BRN_DEBUG("Source of request. Response timeout");

}

void
TopologyDetection::send_response(void)
{
  TopologyDetection::TopologyDetectionForwardInfo *tdi = tdfi_list[0];
  TopologyDetection::TopologyDetectionReceivedInfo *tdri = &(tdi->_last_hops[0]);

  BRN_DEBUG("Send Response to %s.",tdri->_addr.unparse().c_str());

  //TODO:ttl
  WritablePacket *p = TopologyDetectionProtocol::new_backwd_packet(&(tdi->_src), tdi->_id, _node_identity->getMasterAddress(), &(tdri->_addr), NULL);
  output(0).push(p);
}

TopologyDetection::TopologyDetectionForwardInfo *
TopologyDetection::get_forward_info(EtherAddress *src, uint32_t id)
{
  for ( int i = 0; i < tdfi_list.size(); i++ ) {
    if ( tdfi_list[i]->equals(src,id) ) return tdfi_list[i];
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
TopologyDetection::get_bridge_links(Vector<EtherAddress> *_bridge_links)
{
  return;
}


/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

String
TopologyDetection::local_topology_info(void)
{
  StringAccum sa;
  TopologyDetection::TopologyDetectionForwardInfo *tdfi;
  TopologyDetectionReceivedInfo *tdri;

  sa << "<topology_detection node=\"" << _node_identity->getMasterAddress()->unparse() << "\" >\n";
  sa << "\t<forward_info_list size=\"" << tdfi_list.size() << "\" >\n";

  for( int i = 0; i < tdfi_list.size(); i++ )
  {
    tdfi = tdfi_list[i];

    sa << "\t\t<forward_info number=\"" << i <<  "\" src=\"" << tdfi->_src.unparse() << "\" id=\"" << tdfi->_id << "\" ";
    sa << "ttl=\"" << (uint32_t)tdfi->_ttl << "\" >\n";
    sa << "\t\t\t<last_hops size=\"" << tdfi->_last_hops.size() << "\" >\n";
    for( int j = 0; j < tdfi->_last_hops.size(); j++ ) {
      tdri = &(tdfi->_last_hops[j]);
      sa << "\t\t\t\t<hop addr=\"" << tdri->_addr.unparse() << "\" ttl=\"" << tdri->_ttl << "\" ";
      sa << "over_me=\"" << tdri->_over_me << "\" descendant=\"" << tdri->_descendant << "\" />\n";
    }
    sa << "\t\t\t</last_hops>\n\t\t</forward_info>\n";
  }
  sa << "\t</forward_info_list>\n</topology_detection>\n";

  return sa.take_string();
}


enum { H_START_DETECTION, H_TOPOLOGY_INFO };

static int
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  TopologyDetection *f = (TopologyDetection *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_START_DETECTION: {    //debug
      int start;
      if (!cp_integer(s, &start))
        return errh->error("start parameter must be Integer");
      f->start_detection();
      break;
    }
  }
  return 0;
}

static String
read_param(Element *e, void *thunk)
{
  TopologyDetection *t_detect = (TopologyDetection *)e;

  switch ((uintptr_t) thunk)
  {
    case H_TOPOLOGY_INFO : return ( t_detect->local_topology_info() );
    default: return String();
  }
}

void
TopologyDetection::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("detect", write_param, (void *) H_START_DETECTION);

  add_read_handler("local_topo_info", read_param, (void *) H_TOPOLOGY_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TopologyDetection)
