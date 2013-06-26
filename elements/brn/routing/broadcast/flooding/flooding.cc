/* Copyright (C) 2005 BerlifnRoofNet Lab
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
 * flooding.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/wifi/brnwifi.hh"

#include "flooding.hh"
#include "floodingpolicy/floodingpolicy.hh"
#include "../../identity/brn2_nodeidentity.hh"
#include "../../../brnprotocol/brnprotocol.hh"
#include "piggyback/flooding_piggyback.hh"

CLICK_DECLS

Flooding::Flooding()
  : _flooding_passiveack(NULL),
    _bcast_id(0),
    _flooding_src(0),
    _flooding_rx(0),
    _flooding_sent(0),
    _flooding_fwd(0),
    _flooding_passive(0),
    _flooding_last_node_due_to_passive(0),
    _flooding_last_node_due_to_ack(0),
    _flooding_last_node_due_to_piggyback(0),
    _flooding_lower_layer_reject(0),
    _flooding_src_new_id(0),
    _flooding_rx_new_id(0),
    _flooding_fwd_new_id(0)
{
  BRNElement::init();
}

Flooding::~Flooding()
{
  reset();
}

int
Flooding::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "FLOODINGPOLICY", cpkP+cpkM, cpElement, &_flooding_policy,
      "FLOODINGPASSIVEACK", cpkP, cpElement, &_flooding_passiveack,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
static_retransmit_broadcast(BRNElement *e, Packet *p, EtherAddress *src, uint16_t bcast_id)
{
  return ((Flooding*)e)->retransmit_broadcast(p, src, bcast_id);
}

int
Flooding::initialize(ErrorHandler *)
{
  if (_flooding_passiveack != NULL ) {
    _flooding_passiveack->set_retransmit_bcast(this, static_retransmit_broadcast);
    _flooding_passiveack->set_flooding((Flooding*)this);
  }
  return 0;
}

/**
 * 
 * @param port 
 * @param packet 
 */
void
Flooding::push( int port, Packet *packet )
{
  BRN_DEBUG("Flooding: PUSH\n");

  struct click_brn_bcast *bcast_header;
  EtherAddress src;

  uint8_t ttl = BRNPacketAnno::ttl_anno(packet);

  if ( port == 0 )                       // kommt von Client (arp oder so)
  {
    Timestamp now = Timestamp::now();
    BRN_DEBUG("Flooding: PUSH vom Client\n");

    BRN_DEBUG("New Broadcast from %s. ID: %d",src.unparse().c_str(),_bcast_id);

    click_ether *ether = (click_ether *)packet->data();
    src = EtherAddress(ether->ether_shost);

    _bcast_id++;
    if ( _bcast_id == 0 ) _bcast_id = 1;
    
    _flooding_src++;                                                           //i was src of a flooding
    _flooding_src_new_id++;   
    
    add_broadcast_node(&src);
    add_id(&src,(uint32_t)_bcast_id, &now, true);                              //add id for src and i'm src
    forward_attempt(&src, _bcast_id);                                          //try forward 
    add_last_node(&src,(int32_t)_bcast_id, &src, true);                        //add src as last hop for src

    Vector<EtherAddress> forwarder;
    Vector<EtherAddress> passiveack;
    extra_data_size = BCAST_MAX_EXTRA_DATA_SIZE;

    _flooding_policy->init_broadcast(&src,(uint32_t)_bcast_id, 
                                     &extra_data_size, extra_data, &forwarder, &passiveack);

    if ( extra_data_size == BCAST_MAX_EXTRA_DATA_SIZE ) extra_data_size = 0;

    BRN_DEBUG("Extra Data Init: %d",extra_data_size);

    packet->pull(6);                                                           //remove mac Broadcast-Address

    WritablePacket *new_packet = packet->push(sizeof(struct click_brn_bcast) + extra_data_size); //add BroadcastHeader

    bcast_header = (struct click_brn_bcast *)(new_packet->data());
    bcast_header->bcast_id = htons(_bcast_id);
    bcast_header->flags = 0;
    bcast_header->extra_data_size = extra_data_size;

    if ( extra_data_size > 0 ) memcpy((uint8_t*)&(bcast_header[1]), extra_data, extra_data_size);

    if ( ttl == 0 ) ttl = DEFAULT_TTL;

    WritablePacket *out_packet = BRNProtocol::add_brn_header(new_packet, BRN_PORT_FLOODING, BRN_PORT_FLOODING,
                                                                         ttl, DEFAULT_TOS);
    BRNPacketAnno::set_ether_anno(out_packet, brn_ethernet_broadcast, brn_ethernet_broadcast, ETHERTYPE_BRN);
    
    if ( _flooding_passiveack != NULL ) {
      _flooding_passiveack->packet_enqueue(out_packet, &src, _bcast_id, &passiveack, -1);
    }

    output(1).push(out_packet);

  } else if ( port == 1 ) {                                   // kommt von brn

    _flooding_rx++;
    
    click_ether *ether = (click_ether *)packet->ether_header();
    EtherAddress fwd = EtherAddress(ether->ether_shost);
    
    BRN_DEBUG("Flooding: PUSH von BRN\n");

    Timestamp now = packet->timestamp_anno();

    bcast_header = (struct click_brn_bcast *)(packet->data());

    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);
    uint8_t flags = bcast_header->flags;
    uint32_t rxdatasize =  bcast_header->extra_data_size;

    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + rxdatasize]));

    BRN_DEBUG("Src: %s Fwd: %s",src.unparse().c_str(), fwd.unparse().c_str());

    add_broadcast_node(&src);
    
    uint32_t c_fwds;
    bool is_known = have_id(&src, p_bcast_id, &now, &c_fwds);
    ttl--;

    BRN_DEBUG("Fwds: %d",c_fwds);
    
    uint8_t dev_id = BRNPacketAnno::devicenumber_anno(packet);

    Vector<EtherAddress> forwarder;
    Vector<EtherAddress> passiveack;

    extra_data_size = BCAST_MAX_EXTRA_DATA_SIZE;  //the policy can use 256 Byte
    
    uint8_t *rxdata = NULL;
    if ( rxdatasize > 0 ) rxdata = (uint8_t*)&(bcast_header[1]); 

    FloodingPiggyback::bcast_header_get_last_nodes(this, &src, p_bcast_id, rxdata, rxdatasize);
    
    bool forward = (ttl > 0) && _flooding_policy->do_forward(&src, &fwd, _me->getDeviceByNumber(dev_id)->getEtherAddress(), p_bcast_id, is_known, c_fwds,
                                                             rxdatasize/*rx*/, rxdata /*rx*/, &extra_data_size, extra_data,
                                                             &forwarder, &passiveack);

    if ( extra_data_size == BCAST_MAX_EXTRA_DATA_SIZE ) extra_data_size = 0;
    
    BRN_DEBUG("Extra Data Forward: %d in %d out Forward %d", 
              rxdatasize, extra_data_size, (int)(forward?(1):(0)));
    
    if ( ! is_known ) {   //note and send to client only if this is the first time
      Packet *p_client;

      _flooding_rx_new_id++;
      
      add_id(&src,(int32_t)p_bcast_id, &now);

      if ( forward )
        p_client = packet->clone()->uniqueify();                           //packet for client
      else
        p_client = packet;

      if ( sizeof(struct click_brn_bcast) + rxdatasize >= 6 ) {
        p_client->pull((sizeof(struct click_brn_bcast) + rxdatasize) - 6);           //remove bcast_header+extradata, but leave space for target addr
      } else {
        p_client = p_client->push(6 - (sizeof(struct click_brn_bcast) + rxdatasize));//remove bcast_header+extradata, but leave space for target addr
      }
      
      memcpy((void*)p_client->data(), (void*)brn_ethernet_broadcast, 6);//set dest to bcast

      if ( BRNProtocol::is_brn_etherframe(p_client) )
        BRNProtocol::get_brnheader_in_etherframe(p_client)->ttl = ttl;

      output(0).push(p_client);                           // to clients (arp,...)
    }

    //add last hop as last node
    add_last_node(&src,(int32_t)p_bcast_id, &fwd, forward);
    inc_received(&src,(uint32_t)p_bcast_id, &fwd);
    
    //add src of bcast as last node
    if ( add_last_node(&src,(int32_t)p_bcast_id, &src, false) != 0 ) {
      BRN_DEBUG("Add src as last node");
      _flooding_last_node_due_to_piggyback++;
    }
    
    if (forward) {
      _flooding_fwd++;

      forward_attempt(&src, p_bcast_id);
     
      BRN_DEBUG("Forward: %s ID:%d", src.unparse().c_str(), p_bcast_id);

      if ( rxdatasize > 0 ) packet->pull(rxdatasize);           //remove rx data

      if ( extra_data_size > 0 ) 
        if ( packet->push(extra_data_size) == NULL) //add space for new stuff (txdata)
          BRN_ERROR("packet->push failed");

      bcast_header = (struct click_brn_bcast *)(packet->data());

      bcast_header->bcast_id = htons(p_bcast_id);
      bcast_header->flags = flags;
      bcast_header->extra_data_size = extra_data_size;

      if ( extra_data_size > 0 ) memcpy((uint8_t*)&(bcast_header[1]), extra_data, extra_data_size); 

      WritablePacket *out_packet = BRNProtocol::add_brn_header(packet, BRN_PORT_FLOODING, BRN_PORT_FLOODING, ttl, DEFAULT_TOS);
      BRNPacketAnno::set_ether_anno(out_packet, brn_ethernet_broadcast, brn_ethernet_broadcast, ETHERTYPE_BRN);

      if ( _flooding_passiveack != NULL ) {
        _flooding_passiveack->packet_enqueue(out_packet, &src, p_bcast_id, &passiveack, -1);
      }

      output(1).push(out_packet);

    } else {
      BRN_DEBUG("No forward: %s:%d",src.unparse().c_str(), p_bcast_id);
      if (is_known) packet->kill();  //no forwarding and already known (no forward to client) , so kill it
    }
  } else if ( ( port == 2 ) || ( port == 3 ) ) { //txfeedback failure or success
    uint8_t devicenr = BRNPacketAnno::devicenumber_anno(packet);
    
    click_ether *ether = (click_ether *)packet->ether_header();
    EtherAddress rx_node = EtherAddress(ether->ether_dhost);  //target of unicast has the packet
    
    //BRN_ERROR("RX: %s %d %d", rx_node.unparse().c_str(),_me->getDeviceByNumber(devicenr)->getDeviceType(), devicenr );
       
    bcast_header = (struct click_brn_bcast *)(packet->data());
    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + bcast_header->extra_data_size]));
    BRN_DEBUG("Src: %s",src.unparse().c_str());

    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);
    
    //TODO: maybe last node is already known due to other ....whatever
    
    forward_done(&src, p_bcast_id, (port == 3) && (!rx_node.is_broadcast()), get_last_node(&src, p_bcast_id, &rx_node) == NULL);
    
    if ((!rx_node.is_broadcast()) && (_me->getDeviceByNumber(devicenr)->getDeviceType() == DEVICETYPE_WIRELESS)) {
      struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(packet);
      _flooding_sent += (((int)ceh->retries) + 1);
      BRN_DEBUG("Unicast");
      sent(&src, p_bcast_id, (((int)ceh->retries) + 1));
    } else {
      _flooding_sent++;
      sent(&src, p_bcast_id, 1);
    }

    if ( port == 2 ) { //txfeedback failure  
      BRN_DEBUG("Flooding: TXFeedback failure\n");
    } else {           //txfeedback success
      BRN_DEBUG("Flooding: TXFeedback success\n");
      if (!rx_node.is_broadcast()) add_last_node(&src,(int32_t)p_bcast_id, &rx_node, false);
    }
    
    packet->kill();
      
  } else if ( port == 4 ) { //passive overhear
    BRN_DEBUG("Flooding: Passive Overhear\n");
    
    _flooding_passive++;

    uint8_t devicenr = BRNPacketAnno::devicenumber_anno(packet);
    click_ether *ether = (click_ether *)packet->ether_header();
    EtherAddress rx_node = EtherAddress(ether->ether_dhost);  //target of unicast has the packet
    
    //TODO: what if it not wireless. Packet transmission always successful ?
    if ((!rx_node.is_broadcast()) && (_me->getDeviceByNumber(devicenr)->getDeviceType() == DEVICETYPE_WIRELESS)) {

      struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(packet);

      bcast_header = (struct click_brn_bcast *)(packet->data());
      uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);
      src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + bcast_header->extra_data_size]));


      if ( (ceh->flags & WIFI_EXTRA_FOREIGN_TX_SUCC) != 0 ) {
        BRN_ERROR("Unicast: %s has successfully receive ID: %d from %s.",rx_node.unparse().c_str(), p_bcast_id, src.unparse().c_str());

        Timestamp now = packet->timestamp_anno();
        add_id(&src,(int32_t)p_bcast_id, &now);

        if ( add_last_node(&src,(int32_t)p_bcast_id, &rx_node, false) > 0 ) {
          BRN_ERROR("Add new node to last node due to passive unicast...");
          _flooding_last_node_due_to_passive++;
        }
      } else { //packet was not successfully transmitted (we can not be sure)
        if ( get_last_node(&src, (int32_t)p_bcast_id, &rx_node) == NULL ) { //node is not a last node yet so add as assigned
          BRN_ERROR("Assign new node...");
          //assign_last_node(&src, (uint32_t)p_bcast_id, &rx_node);
        }
      }
    }

    push(1, packet);

  } else if ( port == 5 ) {  //low layer reject (e.g. unicast reject transmission)        
    bcast_header = (struct click_brn_bcast *)(packet->data());
    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + bcast_header->extra_data_size]));
    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);

    BRN_DEBUG("Reject: Src: %s",src.unparse().c_str());
     
    forward_done(&src, p_bcast_id, false);
    
    _flooding_lower_layer_reject++;
   
    packet->kill();
  }
}

void
Flooding::add_broadcast_node(EtherAddress *src)
{
  if ( _bcast_map.findp(*src) == NULL )
    _bcast_map.insert(*src, new BroadcastNode(src));
}

Flooding::BroadcastNode*
Flooding::get_broadcast_node(EtherAddress *src)
{
  if ( _bcast_map.findp(*src) == NULL ) {
    BRN_DEBUG("Couldn't find %s",src->unparse().c_str());
    return NULL;
  }
  return _bcast_map.find(*src);
}

void
Flooding::add_id(EtherAddress *src, uint32_t id, Timestamp *now, bool me_src)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_DEBUG("Add BCASTNODE");
    bcn = new BroadcastNode(src);
    _bcast_map.insert(*src, bcn);
  }
  
  bcn->add_id(id, *now, me_src);
}

int
Flooding::add_last_node(EtherAddress *src, uint32_t id, EtherAddress *last_node, bool forwarded)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_ERROR("BCastNode is unknown. Discard info.");
    return -1;
  }

  return bcn->add_last_node(id, last_node, forwarded);
}

void
Flooding::inc_received(EtherAddress *src, uint32_t id, EtherAddress *last_node)
{
  uint32_t *cnt = _recv_cnt.findp(*last_node);
  if ( cnt == NULL ) _recv_cnt.insert(*last_node,1);
  else (*cnt)++;

  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_ERROR("BCastNode is unknown. Discard info.");
    return;
  }

  bcn->add_recv_last_node(id, last_node);
}

bool
Flooding::have_id(EtherAddress *src, uint32_t id, Timestamp *now, uint32_t *count_forwards)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    *count_forwards = 0;
    return false;
  }

  *count_forwards = bcn->forward_attempts(id);

  return bcn->have_id(id,*now);
}

void
Flooding::forward_attempt(EtherAddress *src, uint32_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;

  bcn->forward_attempt(id);
}

void
Flooding::forward_done(EtherAddress *src, uint32_t id, bool success, bool new_node)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;
  if (( bcn->forward_done_cnt(id) == 0 ) && (!me_src(src, id))) _flooding_fwd_new_id++;
  
  bcn->forward_done(id, success);
  if (success && new_node) _flooding_last_node_due_to_ack++;
}

uint32_t
Flooding::unfinished_forward_attempts(EtherAddress *src, uint32_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  
  if ( bcn == NULL ) return 0;
  
  return bcn->unfinished_forward_attempts(id);
}
 
void
Flooding::sent(EtherAddress *src, uint32_t id, uint32_t no_transmission)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return;

  bcn->sent(id, no_transmission);
}

bool
Flooding::me_src(EtherAddress *src, uint32_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) return false;
  
  return bcn->me_src(id);
}

int
Flooding::retransmit_broadcast(Packet *p, EtherAddress *src, uint16_t bcast_id)
{
  BRN_DEBUG("Retransmit: %s %d (%d) %d ",src->unparse().c_str(),bcast_id,p->length(), (uint32_t)p->data()[0]);
  if (me_src(src, bcast_id)) _flooding_src++;
  else _flooding_fwd++;

  forward_attempt(src, bcast_id);

  output(1).push(p);
  
  return 0;
}

void
Flooding::reset()
{
  _flooding_src = _flooding_fwd = _bcast_id = _flooding_rx = _flooding_sent = _flooding_passive = 0;
  _flooding_last_node_due_to_passive = _flooding_last_node_due_to_ack = _flooding_last_node_due_to_piggyback = 0;
  _flooding_lower_layer_reject = _flooding_src_new_id = _flooding_rx_new_id = _flooding_fwd_new_id = 0;

  if ( _bcast_map.size() > 0 ) {
    BcastNodeMapIter iter = _bcast_map.begin();
    while (iter != _bcast_map.end())
    {
      BroadcastNode* bcn = iter.value();
      delete bcn;
      iter++;
    }

    _bcast_map.clear();
  }
}

struct Flooding::BroadcastNode::flooding_last_node*
Flooding::get_last_node(EtherAddress *src, uint32_t id, EtherAddress *last)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn == NULL ) return NULL;
  return bcn->get_last_node(id, last);
}

struct Flooding::BroadcastNode::flooding_last_node*
Flooding::get_last_nodes(EtherAddress *src, uint32_t id, uint32_t *size)
{
  *size = 0;
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->get_last_nodes(id, size);
  return NULL;
}

void
Flooding::assign_last_node(EtherAddress *src, uint32_t id, EtherAddress *last_node)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  
  if ( bcn == NULL ) return;

  bcn->assign_node(id, last_node);
}

struct Flooding::BroadcastNode::flooding_last_node*
Flooding::get_assigned_nodes(EtherAddress *src, uint32_t id, uint32_t *size)
{
  *size = 0;
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->get_assigned_nodes(id, size);
  return NULL;
}

String
Flooding::stats()
{
  StringAccum sa;

  sa << "<flooding node=\"" << BRN_NODE_NAME << "\" policy=\"" <<  _flooding_policy->floodingpolicy_name();
  sa << "\" >\n\t<localstats source=\"" << _flooding_src << "\" received=\"" << _flooding_rx;
  sa << "\" sent=\"" << _flooding_sent << "\" forward=\"" << _flooding_fwd;
  sa << "\" passive=\"" << _flooding_passive << "\" last_node_passive=\"" << _flooding_last_node_due_to_passive;
  sa << "\" last_node_ack=\"" << _flooding_last_node_due_to_ack << "\" last_node_piggyback=\"" << _flooding_last_node_due_to_piggyback;
  sa << "\" low_layer_reject=\"" << _flooding_lower_layer_reject;
  sa << "\" source_new=\"" << _flooding_src_new_id << "\" forward_new=\"" << _flooding_fwd_new_id;
  sa << "\" received_new=\"" << _flooding_rx_new_id << "\" />\n\t<neighbours count=\"" << _recv_cnt.size() << "\" >\n";
  
   for (RecvCntMapIter rcm = _recv_cnt.begin(); rcm.live(); ++rcm) {
    uint32_t cnt = rcm.value();
    EtherAddress addr = rcm.key();
        
    sa << "\t\t<node addr=\"" << addr.unparse() << "\" rcv_cnt=\"" << cnt << "\" />\n";
  }
  
  sa << "\t</neighbours>\n</flooding>\n";

  return sa.take_string();
}

String
Flooding::table()
{
  StringAccum sa;

  sa << "<flooding_table node=\"" << BRN_NODE_NAME << "\">\n";
  BcastNodeMapIter iter = _bcast_map.begin();
  while (iter != _bcast_map.end())
  {
    BroadcastNode* bcn = iter.value();
    int id_c = 0;
    for( uint32_t i = 0; i < DEFAULT_MAX_BCAST_ID_QUEUE_SIZE; i++ )
       if ( bcn->_bcast_id_list[i] != 0 ) id_c++;
 
    sa << "\t<src node=\"" << bcn->_src.unparse() << "\" id_count=\"" << id_c << "\">\n";
    for( uint32_t i = 0; i < DEFAULT_MAX_BCAST_ID_QUEUE_SIZE; i++ ) {
      if ( bcn->_bcast_id_list[i] == 0 ) continue;
      struct BroadcastNode::flooding_last_node *flnl = bcn->_last_node_list[i];
      sa << "\t\t<id value=\"" << bcn->_bcast_id_list[i] << "\" fwd=\"";
      sa << (uint32_t)bcn->_bcast_fwd_list[i] << "\" fwd_done=\"";
      sa << (uint32_t)bcn->_bcast_fwd_done_list[i] << "\" fwd_succ=\"";
      sa << (uint32_t)bcn->_bcast_fwd_succ_list[i] <<	"\" sent=\"";
      sa << (uint32_t)bcn->_bcast_snd_list[i] << "\" >\n";

      for ( int j = 0; j < bcn->_last_node_list_size[i]; j++ ) {
        sa << "\t\t\t<lastnode addr=\"" << EtherAddress(flnl[j].etheraddr).unparse() << "\" forwarded=\"";
        sa << (uint32_t)(flnl[j].flags & FLOODING_LAST_NODE_FLAGS_FORWARDED) << "\" rcv_cnt=\"";
        sa << (uint32_t)(flnl[j].received_cnt) <<"\" />\n";
      }  

      sa << "\t\t</id>\n";

    }
    sa << "\t</src>\n";
    iter++;
  }
  sa << "</flooding_table>\n";

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_stats_param(Element *e, void *)
{
  return ((Flooding *)e)->stats();
}

static String
read_table_param(Element *e, void *)
{
  return ((Flooding *)e)->table();
}

static int 
write_reset_param(const String &/*in_s*/, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  ((Flooding *)e)->reset();

  return 0;
}

void
Flooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);
  add_read_handler("forward_table", read_table_param, 0);

  add_write_handler("reset", write_reset_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Flooding)
