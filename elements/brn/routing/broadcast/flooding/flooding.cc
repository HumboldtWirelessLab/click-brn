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
#include "piggyback/flooding_piggyback.hh"

CLICK_DECLS

Flooding::Flooding()
  : _me(NULL),
    _fhelper(NULL),
    _flooding_db(NULL),
    _flooding_policy(NULL),
    _flooding_passiveack(NULL),
    _bcast_id(0),
    _extra_data_size(0),
    _passive_last_node(false),
    _passive_last_node_rx_acked(false),
    _passive_last_node_assign(false),
    _passive_last_node_foreign_responsibility(false),
    _flooding_src(0),
    _flooding_rx(0),
    _flooding_sent(0),
    _flooding_fwd(0),
    _flooding_passive(0),
    _flooding_passive_acked_dst(0),
    _flooding_passive_not_acked_dst(0),
    _flooding_passive_not_acked_force_dst(0),
    _flooding_node_info_new_finished(0),
    _flooding_node_info_new_finished_src(0),
    _flooding_node_info_new_finished_dst(0),
    _flooding_node_info_new_finished_piggyback(0),
    _flooding_node_info_new_finished_piggyback_resp(0),
    _flooding_node_info_new_finished_passive_src(0),
    _flooding_node_info_new_finished_passive_dst(0),
    _flooding_src_new_id(0),
    _flooding_rx_new_id(0),
    _flooding_fwd_new_id(0),
    _flooding_rx_ack(0),
    _flooding_lower_layer_reject(0),
    _abort_tx_mode(0),
    _tx_aborts(0),
    _tx_aborts_errors(0),
    _scheme_array(NULL),
    _max_scheme_id(0),
    _flooding_strategy(0),
    _rd_queue(NULL)
{
  BRNElement::init();
  reset_last_tx();
  memset(&extra_data,0,sizeof(extra_data));
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
      "FLOODINGPOLICIES", cpkP+cpkM, cpString, &_scheme_string,
      "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
      "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
      "FLOODINGSTRATEGY", cpkP+cpkM, cpInteger, &_flooding_strategy,
      "FLOODINGPASSIVEACK", cpkP, cpElement, &_flooding_passiveack,
      "ABORTTX", cpkP, cpInteger, &_abort_tx_mode,
      "QUEUE", cpkP, cpElement, &_rd_queue,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

/**
 * @brief func is used to allow other element (passive ack,...) to retransmit a packet using flooding element
 *
 * @param e flooding element
 * @param p bcast packet
 * @param src src of bcast
 * @param bcast_id is of bcast
 * @return int error code
 */
static int
static_retransmit_broadcast(BRNElement *e, Packet *p, EtherAddress *src, uint16_t bcast_id)
{
  return(reinterpret_cast<Flooding*>(e))->retransmit_broadcast(p, src, bcast_id);
}

/**
* @brief init flooding element
* 
* @param errh handles errors
* @return int error code
*/
int
Flooding::initialize(ErrorHandler *errh)
{
  if (_flooding_passiveack != NULL ) {
    _flooding_passiveack->set_retransmit_bcast(this, static_retransmit_broadcast);
  }

  parse_schemes(_scheme_string, errh);
  _flooding_policy = get_flooding_scheme(_flooding_strategy);

  _flooding_policy->set_flooding(dynamic_cast<Flooding*>(this));

  return 0;
}

/**
 * @brief The Flooding element has several inputs ( and outputs (2)
 *  Inputs:
 *   0: From Client and click internal packets (packets to flood)
 *   1: BRN-Packet, sent by other nodes and received direct (broadcast or as destination)
 *   2: Feedback failure (incl. broadcast and tx_abort)
 *   3: Feedback success
 *   4: passive overhear
 *   5: low_layer_reject
 * @param port
 * @param packet
 */
void
Flooding::push( int port, Packet *packet )
{
  BRN_DEBUG("Flooding: PUSH\n");

  const click_ether *ether;
  struct click_brn_bcast *bcast_header;

  EtherAddress src;
  EtherAddress rx_node;
  EtherAddress fwd;

  uint16_t p_bcast_id;

  uint8_t devicenr = BRNPacketAnno::devicenumber_anno(packet);
  uint8_t ttl = BRNPacketAnno::ttl_anno(packet);

  int result;

  if ( port != 0 ) {
    ether = reinterpret_cast<const click_ether *>(packet->ether_header());
    rx_node = EtherAddress(ether->ether_dhost);  //target of unicast has the packet
    fwd = EtherAddress(ether->ether_shost);

    bcast_header = (struct click_brn_bcast *)(packet->data());
    src = EtherAddress((uint8_t*)&(packet->data()[sizeof(struct click_brn_bcast) + bcast_header->extra_data_size]));
    p_bcast_id = ntohs(bcast_header->bcast_id);
  }

  if ( port == 0 ) {                      // kommt von Client (arp oder so)

    Timestamp now = Timestamp::now();
    BRN_DEBUG("Flooding: PUSH vom Client\n");

    ether = reinterpret_cast<const click_ether *>(packet->data());
    src = EtherAddress(ether->ether_shost);

    _bcast_id++;
    if ( _bcast_id == 0 ) _bcast_id = 1;

    BRN_DEBUG("New Broadcast from %s. ID: %d",src.unparse().c_str(),_bcast_id);

    _flooding_src_new_id++;

    BroadcastNode *new_bcn;
    new_bcn = _flooding_db->add_broadcast_node(&src);
    _flooding_db->add_id(&src,(uint32_t)_bcast_id, &now, true);                              //add id for src and i'm src

    if ( ! is_local_addr(&src) )
      _flooding_db->add_node_info(&src,(int32_t)_bcast_id, &src, true, true, false, false);  //add src as last hop for src

    // ask policy for init forwarding. this include nodelists for passive ack and forwarder selection (forced fwds)

    Vector<EtherAddress> forwarder;
    Vector<EtherAddress> passiveack;
    _extra_data_size = BCAST_MAX_EXTRA_DATA_SIZE;

    _flooding_policy->init_broadcast(&src,(uint32_t)_bcast_id,
                                     &_extra_data_size, extra_data, &forwarder, &passiveack);

    if ( !forwarder.empty() ) {
      new_bcn->_fix_target_set = true;
      for (Vector<EtherAddress>::iterator i = forwarder.begin(); i != forwarder.end() ; ++i) {
        _flooding_db->add_responsible_node(&src, _bcast_id, i);
      }
    }

    // prepare packet, including extra data
    if ( _extra_data_size == BCAST_MAX_EXTRA_DATA_SIZE ) _extra_data_size = 0;

    BRN_DEBUG("Extra Data Init: %d",_extra_data_size);

    packet->pull(6);                                                           //remove mac Broadcast-Address

    WritablePacket *new_packet = packet->push(sizeof(struct click_brn_bcast) + _extra_data_size); //add BroadcastHeader

    bcast_header = (struct click_brn_bcast *)(new_packet->data());
    bcast_header->bcast_id = htons(_bcast_id);
    bcast_header->flags = 0;
    bcast_header->extra_data_size = _extra_data_size;

    if ( _extra_data_size > 0 ) memcpy((uint8_t*)&(bcast_header[1]), extra_data, _extra_data_size);

    if ( ttl == 0 ) BRNPacketAnno::set_ttl_anno(packet,DEFAULT_TTL);

    if ( _flooding_passiveack != NULL )                   //passiveack will also handle first transmit
      _flooding_passiveack->packet_enqueue(packet, &src, _bcast_id, &passiveack, -1);
    else
      retransmit_broadcast(packet, &src, _bcast_id);      //send packet

  } else if ( port == 1 ) {                               // kommt von brn

    _flooding_rx++;

    BRN_DEBUG("Flooding: PUSH von BRN\n");

    Timestamp now = packet->timestamp_anno();

    uint8_t flags = bcast_header->flags;
    uint32_t rxdatasize = bcast_header->extra_data_size;

    BRN_DEBUG("Src/ID: %s:%d Fwd: %s",src.unparse().c_str(),p_bcast_id , fwd.unparse().c_str());

    BroadcastNode *new_bcn = _flooding_db->add_broadcast_node(&src);

    uint32_t c_fwds;
    bool is_known = _flooding_db->have_id(&src, p_bcast_id, &now, &c_fwds);
    BRN_DEBUG("Fwds: %d",c_fwds);

    ttl--;
    BRNPacketAnno::set_ttl_anno(packet,ttl);

    /**
     *      Add unknown nodes
     *
     * -add id & src & fwd
     *
     **/

    if ( ! is_known ) {   //note only if this is the first time
      _flooding_rx_new_id++;
      _flooding_db->add_id(&src,(int32_t)p_bcast_id, &now);

      //add src of bcast as last node
      BRN_DEBUG("Add src as last node");
      result = _flooding_db->add_node_info(&src,(int32_t)p_bcast_id, &src, false, true, false, false);

      assert((result & FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED) != 0);
    }

    /**
     *            P I G G Y B A C K
     *
     * TODO: push to piggybackelement
     */

    _extra_data_size = BCAST_MAX_EXTRA_DATA_SIZE;  //the policy can use 256 Byte

    uint8_t *rxdata = NULL;
    if ( rxdatasize > 0 ) rxdata = reinterpret_cast<uint8_t*>(&(bcast_header[1])); 

    /*int abort_reason = */FloodingPiggyback::bcast_header_get_node_infos(this, _flooding_db, &src, p_bcast_id, rxdata, rxdatasize);

    /**
     *            A B O R T
     *
     * mac abort
     * 
     * TODO: why abort here -> merge with queue abort
     */

    if ( is_known ) {
      /**  A B O R T **/
      BRN_INFO("Port 1:  RX: %s %s %d (%s) TX: %s %s %d",fwd.unparse().c_str(), src.unparse().c_str(), p_bcast_id,
                                                         rx_node.unparse().c_str(), _last_tx_dst_ea.unparse().c_str(),
                                                         _last_tx_src_ea.unparse().c_str(),_last_tx_bcast_id);

      //TODO: check, whether Piggyback already abort the transmission
      if ( is_last_tx_id(src, p_bcast_id)) {                  //i try to forward this id/src too
        if ( is_last_tx(fwd, src, p_bcast_id) ) {             //my current target is src (my target is the "fwd")
          BRN_DEBUG("current RX node already has the packet");
          abort_last_tx(fwd, FLOODING_TXABORT_REASON_ACKED);
        } else if (_flooding_passiveack->_fhelper->is_better_fwd(*(_me->getMasterAddress()), fwd, _last_tx_dst_ea)) {
          //the fwd has a better link to my target
          BRN_DEBUG("fwd has better link to current target");
          abort_last_tx(FLOODING_TXABORT_REASON_BETTER_LINK);
        }
      }
    }

    /**
     *            A D D   D A T A
     *
     * -inc received
     */
    BRN_DEBUG("Src: %s fwd: %s rxnode: %s Header: %d", src.unparse().c_str(), fwd.unparse().c_str(), rx_node.unparse().c_str(),
                                                       (uint32_t)(bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST));

    //add last hop as last node
    result = _flooding_db->add_node_info(&src,(int32_t)p_bcast_id, &fwd, false, true, false, false);
    if (result & FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED) {
      _flooding_node_info_new_finished_src++;
      if (_passive_last_node) _flooding_node_info_new_finished_passive_src++;
    }
    _flooding_db->inc_received(&src,(uint32_t)p_bcast_id, &fwd);

    /**
     *
     *                            M E   W A S   U N I C A S T   T A R G E T
     *
     */

     if ( _me->isIdentical(&rx_node) ) new_bcn->set_me_as_unicast_target(p_bcast_id);
    /**
     * Handle                         P A S S I V
     *
     * handle target of packet
     * insert to new, assign or foreign_responsible
     */
    if ( _passive_last_node ) {
      if (_passive_last_node_rx_acked || _passive_last_node_foreign_responsibility) {

        result = _flooding_db->add_node_info(&src,(int32_t)p_bcast_id, &rx_node, false, _passive_last_node_rx_acked,
                                                                                   false, _passive_last_node_foreign_responsibility );

        if ( _passive_last_node_rx_acked ) {
          _flooding_passive_acked_dst++;
          if ((result & FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED) != 0) {
            _flooding_node_info_new_finished++;
            _flooding_node_info_new_finished_dst++;
            _flooding_node_info_new_finished_passive_dst++;
          }
        } else {
          _flooding_passive_not_acked_dst++;

          if ((result & FLOODING_NODE_INFO_RESULT_IS_NEW_FOREIGN_RESPONSIBILITY) != 0) {
            _flooding_passive_not_acked_force_dst++;
          }
        }
      } else if (_passive_last_node_assign) {
        _flooding_passive_not_acked_dst++;
        _flooding_db->assign_last_node(&src, (uint32_t)p_bcast_id, &rx_node);
      }

      _passive_last_node = _passive_last_node_assign = _passive_last_node_rx_acked = _passive_last_node_foreign_responsibility = false;
    }
    /**
     * Add Probability
     *
     */
    // packet was sent by fwd, Source was src with p_bcast_id. just 1 try was received
    add_rx_probability(fwd, src, (int32_t)p_bcast_id, 1);

    /**
     *            F O R W A R D ??
     */
    Vector<EtherAddress> forwarder;
    Vector<EtherAddress> passiveack;

    bool forward = (ttl > 0) && _flooding_policy->do_forward(&src, &fwd, _me->getDeviceByNumber(devicenr)->getEtherAddress(), p_bcast_id,
                                                             is_known, c_fwds, rxdatasize/*rx*/, rxdata /*rx*/, &_extra_data_size,
                                                             extra_data, &forwarder, &passiveack);

    if (forward) _flooding_db->add_node_info(&src,(int32_t)p_bcast_id, &fwd, forward, true, false, false); //set forward flag

    if ( _extra_data_size == BCAST_MAX_EXTRA_DATA_SIZE ) _extra_data_size = 0;

    BRN_DEBUG("Extra Data Forward: %d in %d out Forward %d", 
              rxdatasize, _extra_data_size, (int)(forward?(1):(0)));

    /** ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ **/

    /**
     * Handle                         C L I E N T
     */

    if ( ! is_known ) {   //send to client only if this is the first time

      Packet *p_client;

      if (forward) p_client = packet->clone()->uniqueify();                           //packet for client
      else         p_client = packet;

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

    /** ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ **/
    /**
     *                   R E M O V E   P A C K E T   F R O M   Q U E U E
     *
     * TODO: use packet with shorter route
     */

    if ( is_known && ((_abort_tx_mode & FLOODING_TXABORT_MODE_INCLUDE_QUEUE) != 0)) {

      Packet *queue_packet = search_in_queue(src, p_bcast_id, true);

      if ( queue_packet != NULL ) {
        //BRN_DEBUG("Found p in queue %p",queue_packet);

        //TODO: i've to reset ttl from brn_header. why is ttl-anno == 0 ? which element reset the ttl to 0?
        struct click_brn* brn_h = BRNProtocol::get_brnheader(queue_packet);
        BRNPacketAnno::set_ttl_anno(queue_packet, brn_h->ttl);

        //uint8_t qttl = BRNPacketAnno::ttl_anno(queue_packet);
        //BRN_ERROR("queue TTL: %d",qttl);

        _tx_aborts++;
        _flooding_passiveack->handle_feedback_packet(BRNProtocol::pull_brn_header(queue_packet),
                                                     &src, p_bcast_id, false, true, 0);

        //TODO: clear_responsibility_target if sent == 0
      }
    }

    /** ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ **/
    /**
     *                   F O R W A R D     or   kill
     *
     * Some schemes allow to forward a known ( and maybe already forwarded) packet again: e.g. MPR
     */

    if (forward) {
      BRN_DEBUG("Forward: %s ID:%d", src.unparse().c_str(), p_bcast_id);

      //BRN_ERROR("FWDSIZE: %d", forwarder.size());
      if ( !forwarder.empty() ) {
        new_bcn->_fix_target_set = true;
        for (Vector<EtherAddress>::iterator i = forwarder.begin(); i != forwarder.end(); ++i) {
          BRN_DEBUG("Add node %s %d %s", i->unparse().c_str(), p_bcast_id, src.unparse().c_str());
          _flooding_db->add_responsible_node(&src, p_bcast_id, i);
        }
      }

      if ( rxdatasize > 0 ) packet->pull(rxdatasize);           //remove rx data

      if ( _extra_data_size > 0 )
        if ( packet->push(_extra_data_size) == NULL) //add space for new stuff (txdata)
          BRN_ERROR("packet->push failed");

      bcast_header = (struct click_brn_bcast *)(packet->data());

      bcast_header->bcast_id = htons(p_bcast_id);
      bcast_header->flags = flags;
      bcast_header->extra_data_size = _extra_data_size;

      if ( _extra_data_size > 0 ) memcpy((uint8_t*)&(bcast_header[1]), extra_data, _extra_data_size);

      if ( _flooding_passiveack != NULL )
        _flooding_passiveack->packet_enqueue(packet, &src, p_bcast_id, &passiveack, -1);
      else
        retransmit_broadcast(packet, &src, _bcast_id);      //send packet

    } else {
      BRN_DEBUG("No forward: %s:%d",src.unparse().c_str(), p_bcast_id);
      if (is_known) {
        /* We may already forward the packet but due to new info and new probabilities the
         * scheduling may change and we should inform the passiv_ack element
         */
        if ( _flooding_passiveack != NULL )
          _flooding_passiveack->update_flooding_info(packet, src, p_bcast_id);

        packet->kill();  //no forwarding and already known (no forward to client) , so kill it
      }
    }

    /** ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ **/

  } else if ( ( port == 2 ) || ( port == 3 ) ) { //txfeedback failure or success

    BRN_DEBUG("Src: %s",src.unparse().c_str());

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

    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(packet);

    int no_transmissions = 1;       //in the case of wired
    int no_rts_transmissions = 0;
    int no_retries = (int)(ceh->retries);

    bool packet_is_tx_abort = ((port == 2) && ((ceh->flags & WIFI_EXTRA_TX_ABORT) != 0));
    int packet_is_tx_abort_int_inc = (int)(packet_is_tx_abort?0:1);

    if (_me->getDeviceByNumber(devicenr)->getDeviceType() == DEVICETYPE_WIRELESS) {

      if (ceh->flags & WIFI_EXTRA_EXT_RETRY_INFO) {
        no_rts_transmissions = (int)(ceh->virt_col >> 4);
        no_transmissions = (int)(ceh->virt_col & 15);

        if (no_transmissions != (no_retries + packet_is_tx_abort_int_inc)) {
          BRN_ERROR("notx: %d tx: %d rts: %d Pad: %d (Port: %d Abort: %d Retries: %d Dst: %s AbortInc: %d)",
                                                                    no_transmissions, (int)((int)ceh->virt_col & (int)15),
                                                                    no_rts_transmissions, (int)ceh->virt_col,
                                                                    port, (int)(((ceh->flags & WIFI_EXTRA_TX_ABORT)!=0)?1:0),
                                                                    no_retries, rx_node.unparse().c_str(), packet_is_tx_abort_int_inc);

          if ( ((ceh->flags & WIFI_EXTRA_DO_RTS_CTS) == 0) && (no_rts_transmissions > 0) ) {
            click_chatter("NoT: %d Retries: %d tx_abort: %d",no_transmissions,(int)ceh->retries,packet_is_tx_abort);
            assert(no_transmissions == ((int)(ceh->retries) + packet_is_tx_abort_int_inc));
          }
        }

      } else {
        //  TODO: handle RTS/CTS without WIFI_EXTRA_EXT_RETRY_INFO
        no_transmissions = (int)(ceh->retries) + packet_is_tx_abort_int_inc;
      }
    }

    //TODO: maybe node is already known due to other ....whatever

    BroadcastNode *bcn = _flooding_db->get_broadcast_node(&src);

    assert(bcn != NULL);

    //TODO: maybe packet was not forwarded (no mac transmission) due to abort
    if (( bcn->forward_done_cnt(p_bcast_id) == 0 ) && (!_flooding_db->me_src(&src, p_bcast_id))) { //never fwd it it doesn't have to
      if ( no_transmissions > 0) {                                                                 //some transmission for that packet (abort)
        _flooding_fwd_new_id++;
      } else {                                                                                     //no transmission for that packet (abort)
        //TODO: wenn wir via piggyback unsere response rumschicken, kommts als foreign_repo zurück. das gibt dann konflikt
        //check for foreign resp and delete own if foreign rsespons is set
        assert(bcn->get_sent(p_bcast_id) == 0);
        if (bcn->guess_foreign_responsibility_target(p_bcast_id, &rx_node)) {
          bcn->clear_responsibility_target(p_bcast_id, &rx_node);
          bcn->set_foreign_responsibility_target(p_bcast_id, &rx_node);
        }
      }
    }

    /**
     * TODO: for speedup use bcn for the next lines !!
     **/
    bool success = ((port == 3) && (!rx_node.is_broadcast()));

    _flooding_db->forward_done(&src, p_bcast_id, success);
    _flooding_sent += no_transmissions;
    _flooding_db->sent(&src, p_bcast_id, no_transmissions, no_rts_transmissions);

    //if it was an unicast transmission with at least one transmission, than inc number of ucast transmission (net layer)
    if ( (no_transmissions > 0) && (!rx_node.is_broadcast())) {
      _flooding_db->inc_unicast_tx_count(&src, p_bcast_id, &rx_node);
    }

    if ( success ) {    //txfeedback success
      BRN_DEBUG("Flooding: TXFeedback success\n");
      _flooding_rx_ack++;
      result = _flooding_db->add_node_info(&src,(int32_t)p_bcast_id, &rx_node, false, true, false, false);
      if ( (result & FLOODING_NODE_INFO_RESULT_IS_NEW_FINISHED) != 0 ) {
        _flooding_node_info_new_finished++;
        _flooding_node_info_new_finished_dst++;
      }
    } else {            //txfeedback failure
      BRN_DEBUG("Flooding: TXFeedback failure or Broadcast, so that it isn't possible to say whether it was succ or not!\n");
    }

    /**
     * Add Probability
     *
     */
    // pakcet was sent by fwd, Source was src with p_bcast_id.
    add_rx_probability(fwd, src, (int32_t)p_bcast_id, no_transmissions);

    /**
     * passive ack
     */
    if ( _flooding_passiveack != NULL )
      _flooding_passiveack->handle_feedback_packet(packet, &src, p_bcast_id, false, packet_is_tx_abort, no_transmissions);
    else
      packet->kill();

  } else if ( port == 4 ) { //passive overhear
    BRN_DEBUG("Flooding: Passive Overhear (Port 4)");

    _flooding_passive++;
    _passive_last_node = true;

    //TODO: what if it not wireless. Packet transmission always successful ?
    if ((!rx_node.is_broadcast()) && (_me->getDeviceByNumber(devicenr)->getDeviceType() == DEVICETYPE_WIRELESS)) {

      struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(packet);

      /**
       * Abort transmission if possible
       */
      _passive_last_node_rx_acked = ((ceh->flags & WIFI_EXTRA_FOREIGN_TX_SUCC) != 0);

      if (_passive_last_node_rx_acked) {                                 //successful transmission ? Yes,...
        if (is_last_tx(rx_node, src, p_bcast_id)) {                                          //abort
          BRN_DEBUG("lasttx match dst of foreign");
          abort_last_tx(rx_node, FLOODING_TXABORT_REASON_ACKED);
        }
      } else {                                                                               //not successfurl but...
        if (is_last_tx(rx_node, src, p_bcast_id)) {
          BRN_DEBUG("lasttx match dst of foreign (unsuccessful)");

          if ((bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST) != 0) {                   //src forces target
            abort_last_tx(rx_node, FLOODING_TXABORT_REASON_FOREIGN_RESPONSIBILITY);
          } else {
            abort_last_tx(rx_node, FLOODING_TXABORT_REASON_ASSIGNED);
          }
        }
      }

      /**
       * Add new node (passive)
       */

      if (_passive_last_node_rx_acked) {                                 //successful transmission ? Yes,...
        BRN_DEBUG("Unicast: %s has successfully receive ID: %d from %s.",rx_node.unparse().c_str(), p_bcast_id, src.unparse().c_str());
        BRN_DEBUG("New node to last node due to passive unicast...");
      } else {                                       //packet was not successfully transmitted (we can not be sure) or is not forced
        BRN_DEBUG("Assign new node...");
        _passive_last_node_foreign_responsibility = ((bcast_header->flags & BCAST_HEADER_FLAGS_FORCE_DST) != 0);
        _passive_last_node_assign = ! _passive_last_node_foreign_responsibility;
      }
    }

    push(1, packet);

  } else if ( port == 5 ) {  //low layer reject (e.g. unicast reject transmission)

    BRN_DEBUG("Reject: Src: %s",src.unparse().c_str());

    _flooding_db->forward_done(&src, p_bcast_id, false);

    _flooding_lower_layer_reject++;

    if ( _flooding_passiveack != NULL )
      _flooding_passiveack->handle_feedback_packet(packet, &src, p_bcast_id, true, false, 0); //reject is not abort and is not transmitted
    else
      packet->kill();
  }
}

int
Flooding::retransmit_broadcast(Packet *p, EtherAddress *src, uint16_t bcast_id)
{
  BRN_DEBUG("Retransmit: %s %d (%d) %d ",src->unparse().c_str(),bcast_id,p->length(), (uint32_t)p->data()[0]);

  if (_flooding_db->me_src(src, bcast_id)) _flooding_src++;
  else _flooding_fwd++;

  _flooding_db->forward_attempt(src, bcast_id);

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
Flooding::add_rx_probability(EtherAddress &fwd, EtherAddress &src, uint16_t id, uint32_t no_transmissions)
{
  BroadcastNode *bcn = _flooding_db->get_broadcast_node(&src);

  CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(fwd);

  for( int n_i = cnml->_neighbors.size()-1; n_i >= 0; n_i--) {
    int pdr = cnml->get_metric(cnml->_neighbors[n_i]);
    bcn->add_probability(id, &(cnml->_neighbors[n_i]), pdr, no_transmissions);
  }
}

void
Flooding::reset()
{
  _flooding_src = _flooding_fwd = _bcast_id = _flooding_rx = _flooding_sent = _flooding_passive = 0;
  _flooding_passive_acked_dst = _flooding_node_info_new_finished = _flooding_passive_not_acked_dst = 0;
  _flooding_lower_layer_reject = _flooding_src_new_id = _flooding_rx_new_id = _flooding_fwd_new_id = 0;
  _flooding_rx_ack = _tx_aborts = _tx_aborts_errors = _flooding_passive_not_acked_force_dst= 0;
  _flooding_node_info_new_finished_src = _flooding_node_info_new_finished_dst = _flooding_node_info_new_finished_piggyback = _flooding_node_info_new_finished_piggyback_resp = 0;
  _flooding_node_info_new_finished_passive_src = _flooding_node_info_new_finished_passive_dst = 0;

  _flooding_db->reset();
}

Packet *
Flooding::search_in_queue(EtherAddress &src, uint16_t id, bool del)
{
  Packet *p = NULL;
  if (_rd_queue == NULL) return NULL;

  int q_size = _rd_queue->size();

  for ( int q_i = 0; q_i < q_size; q_i++) {
    p = _rd_queue->get_packet(q_i);

    struct click_brn_bcast *bcast_header = (struct click_brn_bcast *)(&(p->data()[sizeof(struct click_brn)]));
    uint16_t p_bcast_id = ntohs(bcast_header->bcast_id);

    if ( p_bcast_id == id ) {
      EtherAddress p_src = EtherAddress((uint8_t*)&(p->data()[sizeof(struct click_brn) + sizeof(struct click_brn_bcast) + bcast_header->extra_data_size]));

      if (p_src == src) {
        if (del) _rd_queue->remove_packet(q_i);
        break;
      }
    }
    p = NULL;
  }

  if ( p != NULL ) {
    BRN_INFO("Packet found");
  } else {
    BRN_INFO("Packet not found");
  }

  return p;
}

//-----------------------------------------------------------------------------
// Schemes
//-----------------------------------------------------------------------------

int
Flooding::parse_schemes(String s_schemes, ErrorHandler* errh)
{
  Vector<String> schemes;
  String s = cp_uncomment(s_schemes);

  cp_spacevec(s, schemes);

  _max_scheme_id = 0;

  if ( schemes.size() == 0 ) {
    if ( _scheme_array != NULL ) delete[] _scheme_array;
    _scheme_array = NULL;

    return 0;
  }

  for (uint16_t i = 0; i < schemes.size(); i++) {
    Element *e = cp_element(schemes[i], this, errh);
    FloodingPolicy *flooding_scheme = reinterpret_cast<FloodingPolicy *>(e->cast("FloodingPolicy"));

    if (!flooding_scheme) {
      return errh->error("Element %s is not a 'FloodingPolicy'",schemes[i].c_str());
    } else {
      _schemes.push_back(flooding_scheme);
      if ( _max_scheme_id < (uint32_t)flooding_scheme->floodingpolicy_id())
        _max_scheme_id = flooding_scheme->floodingpolicy_id();
    }
  }

  if ( _scheme_array != NULL ) delete[] _scheme_array;
  _scheme_array = new FloodingPolicy*[_max_scheme_id + 1];

  for ( uint32_t i = 0; i <= _max_scheme_id; i++ ) {
    _scheme_array[i] = NULL; //Default
    for ( uint32_t s = 0; s < (uint32_t)_schemes.size(); s++ ) {
      if ( i == (uint32_t)_schemes[s]->floodingpolicy_id() ) {
        _scheme_array[i] = _schemes[s];
        break;
      }
    }
  }

  return 0;
}

FloodingPolicy *
Flooding::get_flooding_scheme(uint32_t flooding_strategy)
{
  if ( flooding_strategy > _max_scheme_id ) return NULL;
  return _scheme_array[flooding_strategy];
}

//-----------------------------------------------------------------------------
// Stats
//-----------------------------------------------------------------------------

String
Flooding::stats()
{
  StringAccum sa;

  sa << "<flooding node=\"" << BRN_NODE_NAME << "\" policy=\"" <<  _flooding_policy->floodingpolicy_name();
  sa << "\" policy_id=\"" << _flooding_policy->floodingpolicy_id() << "\" tx_abort_mode=\"" << (int)_abort_tx_mode;
  sa << "\" >\n\t<localstats source=\"" << _flooding_src << "\" received=\"" << _flooding_rx;
  sa << "\" sent=\"" << _flooding_sent << "\" forward=\"" << _flooding_fwd;
  sa << "\" passive=\"" << _flooding_passive << "\" passive_acked_dst=\"" << _flooding_passive_acked_dst;
  sa << "\" passive_not_acked_dst=\"" << _flooding_passive_not_acked_dst << "\" passive_force_dst=\"" << _flooding_passive_not_acked_force_dst;
  sa << "\" finished=\"" << _flooding_node_info_new_finished << "\" finished_src=\"" << _flooding_node_info_new_finished_src;
  sa << "\" finished_dst=\"" << _flooding_node_info_new_finished_dst << "\" finished_piggyback=\"" << _flooding_node_info_new_finished_piggyback;
  sa << "\" resp_piggyback=\"" << _flooding_node_info_new_finished_piggyback_resp;
  sa << "\" finished_passive_src=\"" << _flooding_node_info_new_finished_passive_src << "\" finished_passive_dst=\"" << _flooding_node_info_new_finished_passive_dst;

  sa << "\" rx_ack=\"" << _flooding_rx_ack << "\" low_layer_reject=\"" << _flooding_lower_layer_reject;
  sa << "\" source_new=\"" << _flooding_src_new_id << "\" forward_new=\"" << _flooding_fwd_new_id;
  sa << "\" received_new=\"" << _flooding_rx_new_id << "\" txaborts=\"" << _tx_aborts << "\" tx_aborts_errors=\"" << _tx_aborts_errors;
  sa << "\" />\n\t<neighbours count=\"" << _flooding_db->_recv_cnt.size() << "\" >\n";

  for (RecvCntMapIter rcm = _flooding_db->_recv_cnt.begin(); rcm.live(); ++rcm) {
    uint32_t cnt = rcm.value();
    EtherAddress addr = rcm.key();

    sa << "\t\t<node addr=\"" << addr.unparse() << "\" rcv_cnt=\"" << cnt << "\" />\n";
  }

  sa << "\t</neighbours>\n</flooding>\n";

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_stats_param(Element *e, void *)
{
  return(reinterpret_cast<Flooding *>(e))->stats();
}

static int 
write_reset_param(const String &/*in_s*/, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
 (reinterpret_cast<Flooding *>(e))->reset();

  return 0;
}

void
Flooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_stats_param, 0);

  add_write_handler("reset", write_reset_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Flooding)
