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
 * FloodingPrenegotiation.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/standard/compression/lzw.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "floodingprenegotiation.hh"
#include <sys/types.h>


CLICK_DECLS

FloodingPrenegotiation::FloodingPrenegotiation():
  _linkstat(NULL), _link_table(NULL), _flooding_db(NULL),
  _seq(0),
  _max_ids_per_packet(DEFAULT_MAX_FLOODING_IDS_PER_PACKET),
  _node_timeout(DEFAULT_FLOODING_NEIGHBOURS_TIMEOUT){
  BRNElement::init();
}

FloodingPrenegotiation::~FloodingPrenegotiation()
{
  _neighbour_map.clear();
}

int
FloodingPrenegotiation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
      "LINKTABLE", cpkP+cpkM , cpElement, &_link_table,
      "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
tx_handler(void *element, const EtherAddress */*ea*/, char *buffer, int size)
{
  FloodingPrenegotiation *lph = reinterpret_cast<FloodingPrenegotiation*>(element);

  return lph->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  FloodingPrenegotiation *lph = reinterpret_cast<FloodingPrenegotiation*>(element);

  return lph->lpReceiveHandler(ea, buffer, size);
}

int
FloodingPrenegotiation::initialize(ErrorHandler */*errh*/)
{
  _linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_BROADCAST,&tx_handler,&rx_handler);

  return 0;
}

int
FloodingPrenegotiation::lpSendHandler(char *buffer, int size)
{
  Timestamp now = Timestamp::now();

  if (_flooding_db->_recv_cnt.size() == 0) {
    BRN_INFO("No Flooding");
    return 0;
  }

  if ( (uint32_t)size < (sizeof(struct fooding_link_information) + _max_ids_per_packet*sizeof(uint16_t)) ) {
    BRN_DEBUG("Packet to small");
    return 0;
  }

  BRN_DEBUG("Got some Flooding Info. Check age...");

  struct fooding_link_information *fli = (struct fooding_link_information*)buffer;

  BroadcastNode* bcn = _flooding_db->_bcast_map.begin().value();

  memcpy(&(fli->src_ea), bcn->_src.data(), 6);

  fli->no_rx_nodes = 0;  //TODO:

  /* Fill up ids */
  uint16_t id_index = 0;
  uint16_t *id_pointer = (uint16_t*)&(fli[1]);
  uint16_t sum = 0;

  for(uint32_t i = 0; i < MIN(_max_ids_per_packet,DEFAULT_MAX_BCAST_ID_QUEUE_SIZE); i++ ) {
      if ((bcn->_bcast_id_list[i] == 0) ||            //ignore invalid ids and..
           (bcn->_bcast_snd_list[i] == 0)) continue;  //zero time transmissions (not send yet)

      id_pointer[id_index] = htons(bcn->_bcast_id_list[i]);
      id_index++;
      sum += bcn->_bcast_snd_list[i];

      BRN_DEBUG("Add ID %d tx %d", bcn->_bcast_id_list[i], bcn->_bcast_snd_list[i]);
  }

  if ( sum < 5 ) return 0;

  fli->no_src_ids = (uint8_t)id_index;
  fli->sum_transmissions = htons(sum);

  int space_pointer = sizeof(struct fooding_link_information) + id_index*sizeof(uint16_t);

  NodeAgeMapIter iter = _neighbour_map.begin();

  while (((space_pointer + (7 * sizeof(uint8_t))) <= (u_int32_t)size) && iter.live() ) { //enough space fo rmac and rx probability
    /* Fill up macs and RX PROB */
    Timestamp ts = iter.value();                                                         //get timestamp (last rx)

    if ( (now-ts).msecval() <= _node_timeout ) {                                         //timeout? no,..
      EtherAddress ea = iter.key();                                                      //get etheraddr...

      uint16_t pdr = _link_table->get_link_metric(ea, *_linkstat->_dev->getEtherAddress()); //..and pdr
      if ( pdr != BRN_LT_INVALID_LINK_METRIC ) {                                            //is valid pdr?
        memcpy(&(buffer[space_pointer]), ea.data(), 6);                                     //Yes, so push etheraddr into packet
        buffer[space_pointer + 6] = (uint8_t)(pdr >> 6);                                    //...and pdr too.
        space_pointer += 7 * sizeof(uint8_t);                                               //set pointer to next position in packet
        fli->no_rx_nodes++;                                                                 //inc no nodes (we now have one more in p)
      }
    }

    ++iter;
  }

  return space_pointer;
}

int
FloodingPrenegotiation::lpReceiveHandler(EtherAddress *ea, char *buffer, int size)
{
  BRN_DEBUG("Got Flooding Info.: %s", ea->unparse().c_str());
  _neighbour_map.insert(*ea,Timestamp::now());

  struct fooding_link_information *fli = (struct fooding_link_information*)buffer;

  BRN_DEBUG("Src: %s", EtherAddress(fli->src_ea).unparse().c_str());

  BroadcastNode** bcn_p = _flooding_db->_bcast_map.findp(EtherAddress(fli->src_ea));
  if ( bcn_p == NULL ) return 0;

  BroadcastNode* bcn = *bcn_p;

  /* Fill up ids */
  uint16_t id_index = (uint16_t)fli->no_src_ids;
  uint16_t *id_pointer = (uint16_t*)&(fli[1]);
  uint32_t rx_sum = 0;

  for(uint32_t i = id_index; i > 0; i--) {
    BRN_DEBUG("ID: %d",ntohs(id_pointer[i-1]));
    struct BroadcastNode::flooding_node_info* fni = bcn->get_node_info(ntohs(id_pointer[i-1]), ea);
    if (fni) rx_sum += fni->received_cnt;
  }

  BRN_DEBUG("TX-SUM: %d RX-SUM: %d",ntohs(fli->sum_transmissions), rx_sum);

  uint32_t rx_prob = (100*rx_sum)/ntohs(fli->sum_transmissions);

  if ( rx_prob > 0 ) _link_table->update_link(*ea, *(_linkstat->_dev->getEtherAddress()), _seq, 1,
                                              rx_prob << 6, LINK_UPDATE_LOCAL_ACTIVE, false);

  BRN_DEBUG("Prob: %d", rx_prob);

  BRN_DEBUG("Includes: %d",fli->no_rx_nodes);

  int space_pointer = sizeof(struct fooding_link_information) + id_index*sizeof(uint16_t);

  for(uint32_t i = 0; i < fli->no_rx_nodes; i++) {
    /* Get macs */
    /* Get RX PROB */
    EtherAddress rx_ea = EtherAddress((unsigned char*)&(buffer[space_pointer]));
    uint8_t pdr = buffer[space_pointer+6];

    if ( pdr > 0 ) _link_table->update_link( rx_ea, *ea, _seq, 1,
                                            pdr << 6, LINK_UPDATE_LOCAL_ACTIVE, false);

    space_pointer += 7 * sizeof(uint8_t);

    BRN_DEBUG("%s : %d", rx_ea.unparse().c_str(),(uint32_t)pdr);

  }

  assert(space_pointer == size);

  return space_pointer;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
FloodingPrenegotiation::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingPrenegotiation)
