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
dibadawnAlgo(this),
_is_detect_periodically(false),
_probability_of_perriodically_detection(0.8),
_interval_ms(30 * 1000),
_start_rand(20000),
_timer(this),
_info_timer_active(false),
_info_timer(this),
_info_counter(1)
{
  BRNElement::init();
}

TopologyDetection::~TopologyDetection()
{
}

int TopologyDetection::configure(Vector<String> &conf, ErrorHandler *errh)
{
 click_chatter("RDDBG: begin configure"); 
  if (cp_va_kparse(conf, this, errh,
      "TOPOLOGYINFO", cpkP + cpkM, cpElement, &_topoInfo,
      "NODEIDENTITY", cpkP + cpkM, cpElement, &_node_identity,
      "LINKTABLE", cpkP, cpElement, &_lt,
      "DEBUG", cpkP, cpInteger, &_debug,
      "ORIGIN_FORWARD_DELAY", 0, cpBool, &dibadawnAlgo.config.useOriginForwardDelay,
      "VOTING_RULE", 0, cpInteger, &dibadawnAlgo.config.votingRule,
      "MAX_HOPS", 0, cpInteger, &dibadawnAlgo.config.maxHops,
      "MAX_TRAVERSAL_TIME_MS", 0, cpInteger, &dibadawnAlgo.config.maxTraversalTimeMs,
      "IS_DETECTION_PERIODICALLY", 0, cpBool, &_is_detect_periodically,
      "POBABILITY_OF_PREIODICALLY_DETECTION", 0, cpDouble, &_probability_of_perriodically_detection,
      "AP_POBABILITY", 0, cpDouble, &dibadawnAlgo.config.probabilityForAAPAtNet,
      "BR_POBABILITY", 0, cpDouble, &dibadawnAlgo.config.probabilityForBridgeAtNet,
      "DETECTION_INTERVAL_MS", 0, cpInteger, &_interval_ms,
      "RANDOM_START_DELAY_MS", 0, cpInteger, &_start_rand,
      "USE_LINK_STAT", 0, cpBool, &dibadawnAlgo.config.useLinkStatistic,
      "PRINT_AFTER_RUN", 0, cpBool, &dibadawnAlgo.config.isPrintResults,
      "PRINT_INFO_PERIODICALLY", 0, cpBool, &_info_timer_active,
      cpEnd) < 0)
    return(-1);

  dibadawnAlgo.config.debugLevel = _debug;
  dibadawnAlgo.setTopologyInfo(_topoInfo);
  
  return(0);
}

int TopologyDetection::initialize(ErrorHandler *)
{
  //don't move this to configure, since BRNNodeIdenty is not configured
  //completely while configure this element, so set_active can cause
  //seg, fault, while calling BRN_DEBUG in set_active
  
  click_srandom(_node_identity->getMasterAddress()->hashcode());
  
  const EtherAddress *node = _node_identity->getMasterAddress();
  dibadawnAlgo.config.thisNode = *node;
  
  _timer.initialize(this);
  update_periodically_detection_setup();

  _info_timer.initialize(this);
  update_info_timer();
  
  return 0;
}

int TopologyDetection::reconfigure(String &conf, ErrorHandler *errh)
{
  bool is_topologyinfo_configured;
  bool is_nodeidentity_configured;
  bool is_debug_configured;
  bool is_periodicallyexec_configured;
  bool is_interval_configured;
  bool is_info_timer_configured;
  
  if (cp_va_kparse(conf, this, errh,
      "TOPOLOGY_INFO", cpkC, &is_topologyinfo_configured, cpElement, &_topoInfo,
      "NODE_IDENTITY", cpkC, &is_nodeidentity_configured, cpElement, &_node_identity,
      "LINK_TABLE", 0, cpElement, &_lt,
      "DEBUG", cpkC, &is_debug_configured, cpInteger, &_debug,
      "ORIGIN_FORWARD_DELAY", 0, cpBool, &dibadawnAlgo.config.useOriginForwardDelay,
      "VOTING_RULE", 0, cpInteger, &dibadawnAlgo.config.votingRule,
      "MAX_HOPS", 0, cpInteger, &dibadawnAlgo.config.maxHops,
      "MAX_TRAVERSAL_TIME_MS", 0, cpInteger, &dibadawnAlgo.config.maxTraversalTimeMs,
      "IS_DETECTION_PERIODICALLY", cpkC, &is_periodicallyexec_configured, cpBool, &_is_detect_periodically,
      "POBABILITY_OF_PREIODICALLY_DETECTION", 0, cpDouble, &_probability_of_perriodically_detection,
      "AP_POBABILITY", 0, cpDouble, &dibadawnAlgo.config.probabilityForAAPAtNet,
      "BR_POBABILITY", 0, cpDouble, &dibadawnAlgo.config.probabilityForBridgeAtNet,
      "DETECTION_INTERVAL_MS", cpkC, &is_interval_configured, cpInteger, &_interval_ms,
      "RANDOM_START_DELAY_MS", 0, cpInteger, &_start_rand,
      "USE_LINK_STAT", 0, cpBool, &dibadawnAlgo.config.useLinkStatistic,
      "PRINT_AFTER_RUN", 0, cpBool, &dibadawnAlgo.config.isPrintResults,
      "PRINT_INFO_PERIODICALLY", cpkC, &is_info_timer_configured, cpBool, &_info_timer_active,
      cpEnd) < 0)
    return(-1);

  if(is_topologyinfo_configured)
    dibadawnAlgo.setTopologyInfo(_topoInfo);
  if(is_nodeidentity_configured)
    dibadawnAlgo.config.thisNode = *_node_identity->getMasterAddress();
  if(is_debug_configured)
    dibadawnAlgo.config.debugLevel = _debug;
  if(is_periodicallyexec_configured || is_interval_configured)
    update_periodically_detection_setup();
  if(is_info_timer_configured)
    update_info_timer();
  return(0);
}

void TopologyDetection::update_periodically_detection_setup()
{
  _timer.unschedule();
  
  if(! _is_detect_periodically)
    return;
  
  uint32_t start_delay = _start_rand > 0 ? click_random() % _start_rand : 0;
  _timer.schedule_after_msec(start_delay);
  BRN_DEBUG("Timer is new scheduled at %s", _timer.expiry().unparse().c_str());
}

void TopologyDetection::update_info_timer()
{
  _info_timer.unschedule();
  
  if(!_info_timer_active)
    return;

  _info_timer.schedule_after_msec(_interval_ms);
  BRN_DEBUG("Info Timer is new scheduled at %s", _info_timer.expiry().unparse().c_str());
}

void TopologyDetection::push(int /*port*/, Packet *packet)
{
  click_ether *ether_h = (click_ether *) packet->ether_header();
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
  }

  packet->kill();
}

void TopologyDetection::handle_detection(Packet *brn_packet)
{
  DibadawnPacket packet(*brn_packet);
  //click_chatter("<DEBUG name='%s' addr='%s' />", BRN_NODE_ADDRESS.c_str(), BRN_NODE_NAME.c_str());
  dibadawnAlgo.receive(packet);
}

void TopologyDetection::run_timer(Timer *t)
{
  if (t == NULL)
    BRN_ERROR("Timer is NULL");

  if(t == &_timer)
  {
    double threshold = _probability_of_perriodically_detection * 10000;
    double rand = click_random() % 10000;
    if( rand < threshold)
    {
      BRN_DEBUG("Timer: start search");
      dibadawnAlgo.startNewSearch();
    }
    else
    {
      BRN_DEBUG("Timer: don't start search");
    }

    if (_is_detect_periodically)
      _timer.schedule_after_msec(_interval_ms);
  }
  else if(t == &_info_timer)
  {
    dibadawnAlgo.nodeStatistic.printFinalResult(String(_info_counter));
    _info_counter++;    

    if(_info_timer_active)
      _info_timer.schedule_after_msec(_interval_ms);
  }
  
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/
void TopologyDetection::start_detection()
{
  dibadawnAlgo.startNewSearch();
}

void TopologyDetection::stop_periodically_detection_after_next_run()
{
  _is_detect_periodically = false;
  BRN_DEBUG("Timer will stop after next scheduled run at %s", _timer.expiry().unparse().c_str());
}

void TopologyDetection::reset_link_stat()
{
  dibadawnAlgo.link_stat.reset();
}

String TopologyDetection::xml_link_stat()
{
  if(!dibadawnAlgo.config.useLinkStatistic)
    return("Warning: Please configure 'USE_LINK_STAT' with 'true'");
  
  StringAccum sa;
  sa << "<DibadawnLinkStat node='" << BRN_NODE_NAME << "' time='" << Timestamp::now().unparse() << "' >\n";
  sa << dibadawnAlgo.link_stat.asString();
  sa << "</DibadawnLinkStat>\n";
  
  return(sa.take_string());
}

String TopologyDetection::local_topology_info(void)
{
  return(_topoInfo->topology_info());
}

String TopologyDetection::config()
{
  return(dibadawnAlgo.config.asString());
}

enum
{
  H_START_DETECTION, H_TOPOLOGY_INFO, H_CONFIG, H_STOP_SMOOTHLY, H_STAT
};

static int write_param(const String& click_script_parameter, Element *element, void *vparam, ErrorHandler* errh)
{
  TopologyDetection *topo = (TopologyDetection *) element;
  switch ((intptr_t) vparam)
  {
  case H_START_DETECTION:
    topo->start_detection();
    break;
    
  case H_CONFIG:
  {
    String s = cp_uncomment(click_script_parameter);
    topo->reconfigure(s, errh);
    break;
  }
    
  case H_STOP_SMOOTHLY:
    topo->stop_periodically_detection_after_next_run();
    break;
    
  case H_STAT:
    topo->reset_link_stat();
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
    break;
  
  case H_CONFIG:
    return(topo->config());
    break;
    
  case H_STAT:
    return(topo->xml_link_stat());
    break;
    
  default: return String();
  }
}

void TopologyDetection::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("detect", write_param, (void *) H_START_DETECTION);
  
  add_write_handler("config", write_param, (void *) H_CONFIG);
  add_read_handler("config", read_param, (void *) H_CONFIG);
  
  add_read_handler("local_topo_info", read_param, (void *) H_TOPOLOGY_INFO);
  
  add_write_handler("stop_periotically_detection_smoothly", write_param, (void *) H_STOP_SMOOTHLY);
  
  add_read_handler("link_stat", read_param, (void *) H_STAT);
  add_write_handler("reset_link_stat", write_param, (void *) H_STAT);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(TopologyDetection)
