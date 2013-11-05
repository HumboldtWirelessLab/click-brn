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
#include "piggyback/flooding_piggyback.hh"

CLICK_DECLS

Flooding::Flooding()
  : _flooding_passiveack(NULL),
    _bcast_id(0),
    _passive_last_node_new(false),
    _passive_last_node_rx_acked(false),
    _passive_last_node_assign(false),
    _passive_last_node_foreign_responsibility(false),
    _flooding_src(0),
    _flooding_rx(0),
    _flooding_sent(0),
    _flooding_fwd(0),
    _flooding_passive(0),
    _flooding_passive_not_acked_dst(0),
    _flooding_passive_not_acked_force_dst(0),
    _flooding_last_node_due_to_passive(0),
    _flooding_last_node_due_to_ack(0),
    _flooding_last_node_due_to_piggyback(0),
    _flooding_lower_layer_reject(0),
    _flooding_src_new_id(0),
    _flooding_rx_new_id(0),
    _flooding_fwd_new_id(0),
    _flooding_rx_ack(0),
    _abort_tx_mode(0)
{
  BRNElement::init();
  reset_last_tx();
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
      "ABORTTX", cpkP, cpInteger, &_abort_tx_mode,
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

    click_ether *ether = (click_ether *)packet->data();
    src = EtherAddress(ether->ether_shost);

    _bcast_id++;
    if ( _bcast_id == 0 ) _bcast_id = 1;

    BRN_DEBUG("New Broadcast from %s. ID: %d",src.unparse().c_str(),_bcast_id);

    _flooding_src_new_id++;

    BroadcastNode *new_bcn;
    new_bcn = add_broadcast_node(&src);
    add_id(&src,(uint32_t)_bcast_id, &now, true);                              //add id for src and i'm src

    if ( ! is_local_addr(&src) )
      add_last_node(&src,(int32_t)_bcast_id, &src, true, true, false, false);         //add src as last hop for src

    Vector<EtherAddress> forwarder;
    Vector<EtherAddress> passiveack;
    extra_data_size = BCAST_MAX_EXTRA_DATA_SIZE;

    _flooding_policy->init_broadcast(&src,(uint32_t)_bcast_id, 
                                     &extra_data_size, extra_data, &forwarder, &passiveack);

    if ( !forwarder.empty() ) {
      new_bcn->_fix_target_set = true;
      for (Vector<EtherAddress>::iterator i = forwarder.begin(); i != forwarder.end() ; ++i) {
        add_last_node(&src, _bcast_id, i, false, false, true, false);
      }
    }

    if ( extra_data_size == BCAST_MAX_EXTRA_DATA_SIZE ) extra_data_size = 0;

    BRN_DEBUG("Extra Data Init: %d",extra_data_size);

    packet->pull(6);                                                           //remove mac Broadcast-Address

    WritablePacket *new_packet = packet->push(sizeof(struct click_brn_bcast) + extra_data_size); //add BroadcastHeader

    bcast_header = (struct click_brn_bcast *)(new_packet->data());
    bcast_header->bcast_id = htons(_bcast_id);
    bcast_header->flags = 0;
    bcast_header->extra_data_size = extra_data_size;

    if ( extra_data_size > 0 ) memcpy((uint8_t*)&(bcast_header[1]), extra_data, extra_data_size);

    if ( ttl == 0 ) BRNPacketAnno::set_ttl_anno(packet,DEFAULT_TTL);

    if ( _flooding_passiveack != NULL )                       //passiveack will also handle first transmit
      _flooding_passiveack->packet_enqueue(packet, &src, _bcast_id, &passiveack, -1);
    else
      retransmit_broadcast(packet, &src, _bcast_id);      //send packet

  } else if ( port == 1 ) {                                  // kommt von brn

    _flooding_rx++;

    click_ether *ether = (click_ether *)packet->ether_header();
    EtherAddress fwd = EtherAddress(ether->ether_shost);
    EtherAddress rx_node = EtherAddress(ether->ether_dhost);  //target of unicast has the packet

    BRN_DEBUG("Flooding: PUSH von BRN\n");

    Timestamp now = packet->timestamp_anno();

    bcast_header = (struct click_brn_bcast *)(packet->data());

    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);
    uint8_t flags = bcast_header->flags;
    uint32_t rxdatasize =  bcast_header->extra_data_size;

    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + rxdatasize]));

    BRN_DEBUG("Src: %s Fwd: %s",src.unparse().c_str(), fwd.unparse().c_str());

    BroadcastNode *new_bcn;
    new_bcn = add_broadcast_node(&src);

    uint32_t c_fwds;
    bool is_known = have_id(&src, p_bcast_id, &now, &c_fwds);
    ttl--;
    BRNPacketAnno::set_ttl_anno(packet,ttl);

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

    if ( ! is_known ) {   //note only if this is the first time
      _flooding_rx_new_id++;
      add_id(&src,(int32_t)p_bcast_id, &now);
    } else {
      BRN_INFO("Port 1\nRX: %s %s %d (%s)\nTX: %s %s %d",fwd.unparse().c_str(), src.unparse().c_str(), p_bcast_id, rx_node.unparse().c_str(),
                                                          _last_tx_dst_ea.unparse().c_str(), _last_tx_src_ea.unparse().c_str(),_last_tx_bcast_id);
      if ( is_last_tx(fwd, src, p_bcast_id) ) {
        BRN_DEBUG("current RX node already has the packet");
        abort_last_tx(fwd);
      }
    }

    BRN_DEBUG("Src: %s fwd: %s rxnode: %s Header: %d", src.unparse().c_str(), fwd.unparse().c_str(), rx_node.unparse().c_str(), (uint32_t)(bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST));

    //add last hop as last node
    add_last_node(&src,(int32_t)p_bcast_id, &fwd, forward, true, false, false);
    inc_received(&src,(uint32_t)p_bcast_id, &fwd);

    /**
     * Handle                         P A S S I V
     *
     * handle target of packet
     * insert to new, assign or foreign_responsible
     */
    if (_passive_last_node_rx_acked || _passive_last_node_foreign_responsibility) {
      add_last_node(&src,(int32_t)p_bcast_id, &rx_node, false, _passive_last_node_rx_acked, false, _passive_last_node_foreign_responsibility );
    } else if (_passive_last_node_assign) {
      assign_last_node(&src, (uint32_t)p_bcast_id, &rx_node);
    }

     _passive_last_node_new = _passive_last_node_assign = _passive_last_node_rx_acked = _passive_last_node_foreign_responsibility = false;


    /** ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ **/

    if ( ! is_known ) {   //send to client only if this is the first time
      //add src of bcast as last node
      if ( add_last_node(&src,(int32_t)p_bcast_id, &src, false, true, false, false) != 0 ) {
        BRN_DEBUG("Add src as last node");
      }

      Packet *p_client;

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

    if (forward) {
      BRN_DEBUG("Forward: %s ID:%d", src.unparse().c_str(), p_bcast_id);

      BRN_ERROR("FWDSIZE: %d", forwarder.size());
      if ( !forwarder.empty() ) {
        new_bcn->_fix_target_set = true;
        for (Vector<EtherAddress>::iterator i = forwarder.begin(); i != forwarder.end(); ++i) {
          BRN_ERROR("Add node %s %d %s", i->unparse().c_str(), p_bcast_id, src.unparse().c_str());
          int u = add_last_node(&src, p_bcast_id, i, false, false, true, false);
          BRN_ERROR("Number: %d", u);
        }
      }

      if ( rxdatasize > 0 ) packet->pull(rxdatasize);           //remove rx data

      if ( extra_data_size > 0 ) 
        if ( packet->push(extra_data_size) == NULL) //add space for new stuff (txdata)
          BRN_ERROR("packet->push failed");

      bcast_header = (struct click_brn_bcast *)(packet->data());

      bcast_header->bcast_id = htons(p_bcast_id);
      bcast_header->flags = flags;
      bcast_header->extra_data_size = extra_data_size;

      if ( extra_data_size > 0 ) memcpy((uint8_t*)&(bcast_header[1]), extra_data, extra_data_size); 

      if ( _flooding_passiveack != NULL )
        _flooding_passiveack->packet_enqueue(packet, &src, p_bcast_id, &passiveack, -1);
      else 
        retransmit_broadcast(packet, &src, _bcast_id);      //send packet

    } else {
      BRN_DEBUG("No forward: %s:%d",src.unparse().c_str(), p_bcast_id);
      if (is_known) packet->kill();  //no forwarding and already known (no forward to client) , so kill it
    }
  } else if ( ( port == 2 ) || ( port == 3 ) ) { //txfeedback failure or success
    uint8_t devicenr = BRNPacketAnno::devicenumber_anno(packet);

    click_ether *ether = (click_ether *)packet->ether_header();
    EtherAddress rx_node = EtherAddress(ether->ether_dhost);  //target of unicast has the packet

    bcast_header = (struct click_brn_bcast *)(packet->data());
    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + bcast_header->extra_data_size]));
    BRN_DEBUG("Src: %s",src.unparse().c_str());

    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);

    /**
     * check current tx and compare
     */
    BRN_DEBUG("Port 2 3\nRX: %s %s %d\nTX: %s %s %d",rx_node.unparse().c_str(), src.unparse().c_str(),p_bcast_id,
                                                    _last_tx_dst_ea.unparse().c_str(), _last_tx_src_ea.unparse().c_str(),_last_tx_bcast_id);
    if (is_last_tx(rx_node, src, p_bcast_id)) {
      BRN_DEBUG("lasttx match feedback. Succ: %d",(port==3)?1:0);
      reset_last_tx(); //reset current tx since it is finished now
    } else {
      BRN_ERROR("Wrong feedback. doesn't match last_tx: %s %s %d vs %s %s %d",rx_node.unparse().c_str(), src.unparse().c_str(), p_bcast_id,
                                                                              _last_tx_dst_ea.unparse().c_str(), _last_tx_src_ea.unparse().c_str(),
                                                                              _last_tx_bcast_id);
    }

    //TODO: maybe last node is already known due to other ....whatever

    forward_done(&src, p_bcast_id, (port == 3) && (!rx_node.is_broadcast()), get_last_node(&src, p_bcast_id, &rx_node) == NULL);

    bool packet_is_tx_abort = false;
    int no_transmissions = 1;       //in the case of wired or broadcast

    if ((!rx_node.is_broadcast()) && (_me->getDeviceByNumber(devicenr)->getDeviceType() == DEVICETYPE_WIRELESS)) {
      struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(packet);

      BRN_DEBUG("Unicast");

      if ( (port == 2) && (ceh->flags & WIFI_EXTRA_TX_ABORT)) {
        no_transmissions = (int)ceh->retries;
        packet_is_tx_abort = true;
      } else {
        no_transmissions = (int)ceh->retries + 1;
      }
    } //TODO: correct due to tx abort in the case of wired and/or broadcast (now assume 1 transmission)

    _flooding_sent += no_transmissions;
    sent(&src, p_bcast_id, no_transmissions);

    if ( port == 2 ) { //txfeedback failure
      BRN_DEBUG("Flooding: TXFeedback failure\n");
    } else {           //txfeedback success
      BRN_DEBUG("Flooding: TXFeedback success\n");
      _flooding_rx_ack++;
      if (!rx_node.is_broadcast()) add_last_node(&src,(int32_t)p_bcast_id, &rx_node, false, true, false, false);
    }

    if ( _flooding_passiveack != NULL )
      _flooding_passiveack->handle_feedback_packet(packet, &src, p_bcast_id, false, packet_is_tx_abort, no_transmissions);
    else
      packet->kill();

  } else if ( port == 4 ) { //passive overhear
    BRN_DEBUG("Flooding: Passive Overhear\nPort 4");

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

      /**
       * Abort transmission if possible
       */

      if ( (ceh->flags & WIFI_EXTRA_FOREIGN_TX_SUCC) != 0) {                                 //successful transmission ? Yes,...
        if (is_last_tx(rx_node, src, p_bcast_id)) {                                          //abort
          BRN_DEBUG("lasttx match dst of foreign");
          abort_last_tx(rx_node);
        }
      } else {                                                                               //not successfurl but...
        //if ( ! is_responsibility_target(&src, p_bcast_id, &rx_node) ) {                      //i'm not responsible
            if ((is_last_tx(rx_node, src, p_bcast_id)) &&
                (((_abort_tx_mode & FLOODING_TXABORT_MODE_ASSIGNED) != 0) || ((bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST) != 0)) ) {
            BRN_DEBUG("lasttx match dst of foreign (unsuccessful)");
            abort_last_tx(rx_node);
            //since other node do this, we are not responible anymore
            if ( (bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST) != 0) {
              set_foreign_responsibility_target(&src, p_bcast_id, &rx_node);
            }
          }
        //}
      }

      /**
       * Add new node (passive)
       */

      _passive_last_node_new = (get_last_node(&src, (int32_t)p_bcast_id, &rx_node) == NULL); //rx_node is known ???

      if ( (ceh->flags & WIFI_EXTRA_FOREIGN_TX_SUCC) != 0) {                                 //successful transmission ? Yes,...
        BRN_DEBUG("Unicast: %s has successfully receive ID: %d from %s.",rx_node.unparse().c_str(), p_bcast_id, src.unparse().c_str());
        BRN_DEBUG("New node to last node due to passive unicast...");

        if (_passive_last_node_new) {
          _flooding_last_node_due_to_passive++;
          _flooding_last_node_due_to_ack++;
        }

        _passive_last_node_rx_acked = true;

      } else {                                                     //packet was not successfully transmitted (we can not be sure) or is not forced
        BRN_DEBUG("Assign new node...");
        _flooding_passive_not_acked_dst++;

        if ((bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST) != 0) {
          if (_passive_last_node_new) {
            _flooding_last_node_due_to_passive++;
            _flooding_passive_not_acked_force_dst++;
          }
          _passive_last_node_foreign_responsibility = true;
        } else {
          //TODO: assign node only if it is not acked and there is no foreign respon
          _passive_last_node_assign = true;
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

    if ( _flooding_passiveack != NULL )
      _flooding_passiveack->handle_feedback_packet(packet, &src, p_bcast_id, true, false, 0); //reject is not abort and is not transmitted
    else
      packet->kill();
  }
}

Flooding::BroadcastNode*
Flooding::add_broadcast_node(EtherAddress *src)
{
  Flooding::BroadcastNode* bcn = _bcast_map.find(*src);
  if ( bcn == NULL ) {
    bcn = new BroadcastNode(src);
    _bcast_map.insert(*src, bcn);
  }

  return bcn;
}

Flooding::BroadcastNode*
Flooding::get_broadcast_node(EtherAddress *src)
{
  Flooding::BroadcastNode** bnp = _bcast_map.findp(*src);
  if ( bnp == NULL ) {
    BRN_DEBUG("Couldn't find %s",src->unparse().c_str());
    return NULL;
  }
  return *bnp;
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
Flooding::add_last_node(EtherAddress *src, uint32_t id, EtherAddress *last_node, bool forwarded, bool rx_acked, bool responsibility, bool foreign_responsibility)
{
  BroadcastNode *bcn = _bcast_map.find(*src);

  if ( bcn == NULL ) {
    BRN_ERROR("BCastNode is unknown. Discard info.");
    return -1;
  }

  return bcn->add_last_node(id, last_node, forwarded, rx_acked, responsibility, foreign_responsibility);
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

  uint8_t ttl = BRNPacketAnno::ttl_anno(p);

  WritablePacket *out_packet = BRNProtocol::add_brn_header(p, BRN_PORT_FLOODING, BRN_PORT_FLOODING, ttl, DEFAULT_TOS);

  BRNPacketAnno::set_ether_anno(out_packet, brn_ethernet_broadcast, brn_ethernet_broadcast, ETHERTYPE_BRN);

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  if ((ceh != NULL) && (ceh->magic == WIFI_EXTRA_MAGIC)) {
    memset(ceh, 0, sizeof(struct click_wifi_extra));
    ceh->magic = WIFI_EXTRA_MAGIC;
  }

  output(1).push(out_packet);

  return 0;
}

void
Flooding::reset()
{
  _flooding_src = _flooding_fwd = _bcast_id = _flooding_rx = _flooding_sent = _flooding_passive = 0;
  _flooding_last_node_due_to_passive = _flooding_last_node_due_to_ack = _flooding_last_node_due_to_piggyback = 0;
  _flooding_lower_layer_reject = _flooding_src_new_id = _flooding_rx_new_id = _flooding_fwd_new_id = 0;
  _flooding_rx_ack = _tx_aborts = _tx_aborts_errors = 0;

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

  int res = bcn->assign_node(id, last_node);
  BRN_DEBUG("Finished assign node: %s %d",last_node->unparse().c_str(),res);
}

struct Flooding::BroadcastNode::flooding_last_node*
Flooding::get_assigned_nodes(EtherAddress *src, uint32_t id, uint32_t *size)
{
  *size = 0;
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->get_assigned_nodes(id, size);
  return NULL;
}

void
Flooding::clear_assigned_nodes(EtherAddress *src, uint32_t id)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->clear_assigned_nodes(id);  
}

void
Flooding::set_responsibility_target(EtherAddress *src, uint32_t id, EtherAddress *target)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) bcn->set_responsibility_target(id, target);
}

void
Flooding::set_foreign_responsibility_target(EtherAddress *src, uint32_t id, EtherAddress *target)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) bcn->set_foreign_responsibility_target(id, target);
}

bool
Flooding::is_responsibility_target(EtherAddress *src, uint32_t id, EtherAddress *target)
{
  BroadcastNode *bcn = _bcast_map.find(*src);
  if ( bcn != NULL ) return bcn->is_responsibility_target(id, target);
  return false;
}

String
Flooding::stats()
{
  StringAccum sa;

  sa << "<flooding node=\"" << BRN_NODE_NAME << "\" policy=\"" <<  _flooding_policy->floodingpolicy_name();
  sa << "\" policy_id=\"" << _flooding_policy->floodingpolicy_id() << "\" tx_abort_mode=\"" << (int)_abort_tx_mode;
  sa << "\" >\n\t<localstats source=\"" << _flooding_src << "\" received=\"" << _flooding_rx;
  sa << "\" sent=\"" << _flooding_sent << "\" forward=\"" << _flooding_fwd;
  sa << "\" passive=\"" << _flooding_passive << "\" last_node_passive=\"" << _flooding_last_node_due_to_passive;
  sa << "\" last_node_ack=\"" << _flooding_last_node_due_to_ack << "\" passive_no_ack=\"" << _flooding_passive_not_acked_dst;
  sa << "\" passive_no_ack_force_dst=\"" << _flooding_passive_not_acked_force_dst << "\" rx_ack=\"" << _flooding_rx_ack;
  sa << "\" last_node_piggyback=\"" << _flooding_last_node_due_to_piggyback << "\" low_layer_reject=\"" << _flooding_lower_layer_reject;
  sa << "\" source_new=\"" << _flooding_src_new_id << "\" forward_new=\"" << _flooding_fwd_new_id;
  sa << "\" received_new=\"" << _flooding_rx_new_id << "\" txaborts=\"" << _tx_aborts << "\" tx_aborts_errors=\"" << _tx_aborts_errors;
  sa << "\" />\n\t<neighbours count=\"" << _recv_cnt.size() << "\" >\n";

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
        sa << (uint32_t)(flnl[j].flags & FLOODING_LAST_NODE_FLAGS_FORWARDED) << "\" responsible=\"";
        sa << (uint32_t)(((flnl[j].flags & FLOODING_LAST_NODE_FLAGS_RESPONSIBILITY) == 0)?0:1) << "\" finished_responsible=\"";
        sa << (uint32_t)(((flnl[j].flags & FLOODING_LAST_NODE_FLAGS_FINISHED_RESPONSIBILITY) == 0)?0:1) << "\" foreign_responsible=\"";
        sa << (uint32_t)(((flnl[j].flags & FLOODING_LAST_NODE_FLAGS_FOREIGN_RESPONSIBILITY) == 0)?0:1) << "\" rx_acked=\"";
        sa << (uint32_t)(((flnl[j].flags & FLOODING_LAST_NODE_FLAGS_RX_ACKED) == 0)?0:1) << "\" rcv_cnt=\"";
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
