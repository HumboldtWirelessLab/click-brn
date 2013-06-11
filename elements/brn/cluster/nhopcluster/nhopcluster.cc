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
 * nhopcluster.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn/standard/compression/lzw.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "nhopcluster.hh"
#include "nhopclusterprotocol.hh"

CLICK_DECLS

NHopCluster::NHopCluster()
  : _nhop_timer(static_nhop_timer_hook,this),
    _mode(NHOP_MODE_START),
    _cluster_head_selected(false),
    _delay(0)
{
  Clustering::init();
  _send_notification = _fwd_notification = _send_req = _fwd_req = 0;
}

NHopCluster::~NHopCluster()
{
}

int
NHopCluster::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _start = NHOP_DEFAULTSTART;
  _startdelay = NHOP_DEFAULTSTARTDELAY;

  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM , cpElement, &_node_identity,
      "DISTANCE", cpkP+cpkM , cpInteger, &_max_distance,
      "LINKSTAT", cpkP, cpElement, &_linkstat,
      "START", cpkP, cpInteger, &_start,
      "MAXSTARTDELAY", cpkP, cpInteger, &_startdelay,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
tx_handler(void *element, const EtherAddress */*ea*/, char *buffer, int size)
{
  NHopCluster *dcl = (NHopCluster*)element;
  return dcl->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  NHopCluster *dcl = (NHopCluster*)element;
  return dcl->lpReceiveHandler(ea, buffer, size);
}

int
NHopCluster::initialize(ErrorHandler *)
{
  click_brn_srandom();

  _cluster_head = new ClusterHead(_node_identity->getMasterAddress(), _node_identity->getNodeID32(), 0);
  _own_cluster = _cluster_head;

  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_NHPCLUSTER, &tx_handler, &rx_handler);

  _nhop_timer.initialize(this);
  _nhop_timer.schedule_after_msec( _start + ( click_random() % _startdelay ));

  return 0;
}

int
NHopCluster::lpSendHandler(char *buffer, int size)
{
  struct nhopcluster_lp_info info;

  if ( _cluster_head_selected ) {                                       //if clusterhead is valid (available), then
    _cluster_head->getInfo(&info);                                       //put info of current clusterhead into struct
    return NHopClusterProtocol::pack_lp(&info, (uint8_t*)buffer, size); //put struct into linkprobe
  }
  return -1;  //NO info. TODO: check linkstat for returning 0 instead of -1
}

int
NHopCluster::lpReceiveHandler(EtherAddress *src, char *buffer, int size)
{
  int len;
  struct nhopcluster_lp_info info;

  len = NHopClusterProtocol::unpack_lp(&info, (uint8_t*)buffer, size);

  if ( len > 0 ) {
    if ( _cluster_head_selected ) {
      //TODO: choose this if closer; consider loaddistribution among clusters
    } else {
      if ( info.hops < _max_distance ) {
        _cluster_head->setInfo(&info);
        _cluster_head->setBestNextHop(src);
        _cluster_head->_distance = info.hops + 1;
        _cluster_head_selected = true;
      }
    }
  } else {
    BRN_WARN("ERROR while unpack linkprobeinfo");
  }

  // Protokollieren in welchem Cluster sich der Knoten befindet

  return len;
}

void
NHopCluster::static_nhop_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((NHopCluster*)f)->timer_hook();
}

void
NHopCluster::timer_hook()
{
  switch ( _mode ) {
    case NHOP_MODE_START:
      send_request();
      _mode = NHOP_MODE_REQUEST;
      _nhop_timer.schedule_after_msec(5000);
      break;
    case NHOP_MODE_REQUEST:
       delete _cluster_head;
       _cluster_head = new ClusterHead(_node_identity->getMasterAddress(), _node_identity->getNodeID32(), 0);
       _own_cluster = _cluster_head;

      _mode = NHOP_MODE_NOTIFY;
      _cluster_head_selected = true;
      send_notify();
      _nhop_timer.schedule_after_msec(5000);
      break;
    case NHOP_MODE_NOTIFY:
      _mode = NHOP_MODE_CONFIGURED;
      break;
    case NHOP_MODE_CONFIGURED:
    default:
      break;
  }
}

void
NHopCluster::push(int, Packet *p)
{
  switch(NHopClusterProtocol::get_type(p, 0)) {
    case NHOPCLUSTER_PACKETTYPE_MANAGMENT:
    {
      switch ( NHopClusterProtocol::get_operation(p, 0) ) {
        case NHOPCLUSTER_MANAGMENT_REQUEST:
          handle_request(p);
          return;
        case NHOPCLUSTER_MANAGMENT_REPLY:
          handle_reply(p);
          break;
        case NHOPCLUSTER_MANAGMENT_NOTIFICATION:
          handle_notification(p);
          return;
        case NHOPCLUSTER_MANAGMENT_REJECT:
          break;
        case NHOPCLUSTER_MANAGMENT_INFO:
          break;
        default:
          break;
      }
      break;
    }
    case NHOPCLUSTER_PACKETTYPE_ROUTING:
      break;
    case NHOPCLUSTER_PACKETTYPE_LP_INFO:
      break;
    default:
      BRN_WARN("Unknown packettype");
      break;
  }

  p->kill();
}


void
NHopCluster::send_request()
{
  WritablePacket *p_brn, *p;

  p = NHopClusterProtocol::new_request(1, 0, 0);
  p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_NHOPCLUSTER, BRN_PORT_NHOPCLUSTER, _max_distance, 0);
  BRNPacketAnno::set_ether_anno(p_brn, _node_identity->getMasterAddress()->data(), brn_ethernet_broadcast, ETHERTYPE_BRN );
  output(1).push(p_brn);

  _send_req++;
}

void
NHopCluster::send_notify()
{
  WritablePacket *p_brn, *p;

  p = NHopClusterProtocol::new_notify(&_cluster_head->_clusterhead, 1, 0, _cluster_head->_cluster_id);
  p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_NHOPCLUSTER, BRN_PORT_NHOPCLUSTER, _max_distance, 0);
  BRNPacketAnno::set_ether_anno(p_brn, _node_identity->getMasterAddress()->data(), brn_ethernet_broadcast, ETHERTYPE_BRN );
  output(1).push(p_brn);

  _send_notification++;
}

void
NHopCluster::handle_request(Packet *p)
{
  struct nhopcluster_managment *mgt;

  mgt = NHopClusterProtocol::get_mgt(p, 0);

  if ( mgt->hops <= _max_distance ) {
    if ( mgt->hops < _max_distance ) {
      _fwd_req++;
      forward(p);
    } else {
      p->kill();
    }
  } else {
    p->kill();
  }
}

void
NHopCluster::handle_reply(Packet *p)
{
  struct nhopcluster_managment *mgt;

  mgt = NHopClusterProtocol::get_mgt(p, 0);

  if ( mgt->hops <= _max_distance ) {
    if ( _mode == NHOP_MODE_REQUEST ) {
      _cluster_head->setInfo(mgt);              //TODO: seperated all to extra config function ( setClusterHead() )
      _mode = NHOP_MODE_CONFIGURED;
      _cluster_head_selected = true;
    }

    if ( ( _mode == NHOP_MODE_CONFIGURED ) && ( mgt->hops < _cluster_head->_distance ) ) {
      _cluster_head->setInfo(mgt);
    }
  }
}

void
NHopCluster::handle_notification(Packet *p)
{
  /** TODO: send reject if something wrong */
  struct nhopcluster_managment *mgt;

  mgt = NHopClusterProtocol::get_mgt(p, 0);

  if ( mgt->hops <= _max_distance ) {
    if ( ( _mode == NHOP_MODE_REQUEST ) || ( _mode == NHOP_MODE_START ) ) {
      _cluster_head->setInfo(mgt);              //TODO: seperated all to extra config function ( setClusterHead() )
      _mode = NHOP_MODE_CONFIGURED;
      _cluster_head_selected = true;
    }
    if ( ( _mode == NHOP_MODE_CONFIGURED ) && ( mgt->hops < _cluster_head->_distance ) ) {
      click_chatter("Distance: %d -> %d",_cluster_head->_distance,mgt->hops);
      _cluster_head->setInfo(mgt);
    }

    if ( mgt->hops < _max_distance ) {
      _fwd_notification++;
      forward(p);
    } else {
      p->kill();
    }
  } else {
    p->kill();
  }
}

void
NHopCluster::forward(Packet *p)
{
  WritablePacket *p_brn;
  struct nhopcluster_managment *mgt;

  mgt = NHopClusterProtocol::get_mgt(p, 0);
  mgt->hops++;

  p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_NHOPCLUSTER, BRN_PORT_NHOPCLUSTER, _max_distance - mgt->hops, 0);
  BRNPacketAnno::set_ether_anno(p_brn, _node_identity->getMasterAddress()->data(), brn_ethernet_broadcast, ETHERTYPE_BRN );
  output(1).push(p_brn);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
get_mode_string(int mode)
{
  switch (mode) {
    case NHOP_MODE_START:      return "Start";
    case NHOP_MODE_REQUEST:    return "Request";
    case NHOP_MODE_NOTIFY:     return "Notify";
    case NHOP_MODE_CONFIGURED: return "Configured";
  }

  return "Unknown";
}

String
NHopCluster::get_info()
{
  StringAccum sa;

  sa << "Node: " << _node_identity->getNodeName() << " Mode: " << get_mode_string(_mode);
  sa << "\nClusterhead: " << _cluster_head->_clusterhead.unparse() << " Distance: " << _cluster_head->_distance << "\n";
  sa << "S-R: " << _send_req << " F-R: " << _fwd_req << " S-N: " << _send_notification << " F-N: " << _fwd_notification << "\n";
  return sa.take_string();
}

static String
read_info_param(Element *e, void *)
{
  NHopCluster *nc = (NHopCluster *)e;

  return nc->get_info();
}

static String
read_debug_param(Element *e, void *)
{
  NHopCluster *fl = (NHopCluster *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  NHopCluster *fl = (NHopCluster *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
NHopCluster::add_handlers()
{
  Clustering::add_handlers();

  add_read_handler("info", read_info_param, 0);
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NHopCluster)
