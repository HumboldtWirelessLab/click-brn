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

#include "cooperativechannelstats.hh"

CLICK_DECLS

CooperativeChannelStats::CooperativeChannelStats():
  _cst(NULL),
  _msg_timer(this),
  _interval(0),
  _add_neighbours(false)
{
    BRNElement::init();
}

CooperativeChannelStats::~CooperativeChannelStats()
{
    BRN_DEBUG("CooperativeChannelStats::~CooperativeChannelStats ()");
}

int CooperativeChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
    BRN_DEBUG("int CooperativeChannelStats::configure (Vector<String> &conf, ErrorHandler* errh)");
    int ret = cp_va_kparse(conf, this, errh,
                           "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
                           "INTERVAL", cpkP, cpInteger, &_interval,
                           "NEIGHBOURS", cpkP, cpBool, &_add_neighbours,
                           "DEBUG", cpkP, cpInteger, &_debug,
                           cpEnd );

    return ret;
}

int CooperativeChannelStats::initialize(ErrorHandler */*errh*/)
{
    BRN_DEBUG("init");

    click_brn_srandom();

    _msg_timer.initialize(this);

    if(_interval > 0) {
      _msg_timer.schedule_after_msec((click_random() % (_interval - 100)) + 100);
    }

    return 0;
}

void CooperativeChannelStats::run_timer(Timer */*timer*/)
{
    BRN_DEBUG("void CooperativeChannelStats::run_timer (Timer *)");
    _msg_timer.schedule_after_msec(_interval);
    send_message();
}

void CooperativeChannelStats::send_message()
{
    BRN_DEBUG("void CooperativeChannelStats::send_message ()");

    /* get Info */
    ChannelStats::SrcInfoTable *sit = _cst->get_latest_stats_neighbours();
    struct airtime_stats *airstats = _cst->get_latest_stats();

    uint8_t non = 0; //number of available neighbours
    if (_add_neighbours) non = sit->size();

    WritablePacket *p = Packet::make(128, NULL, sizeof(struct cooperative_channel_stats_header) + sizeof(neighbour_airtime_stats) * non, 32);

    /* Header */
    struct cooperative_channel_stats_header *header = (struct cooperative_channel_stats_header *)p->data();

    header->endianess = ENDIANESS_TEST;
    header->flags = 0;
    header->no_neighbours = non;

    /* own stats */
    header->local_stats.stats_id = airstats->stats_id;
    BRN_DEBUG("ID: %d",header->local_stats.stats_id);
    header->local_stats.duration = airstats->duration;

    header->local_stats.hw_rx = airstats->hw_rx;
    header->local_stats.hw_tx = airstats->hw_tx;
    header->local_stats.hw_busy = airstats->hw_busy;

    header->local_stats.mac_rx = airstats->frac_mac_rx;
    header->local_stats.mac_tx = airstats->frac_mac_tx;
    header->local_stats.mac_busy = airstats->frac_mac_busy;

    header->local_stats.tx_pkt_count = airstats->txpackets;
    header->local_stats.tx_byte_count = airstats->tx_bytes;

    header->local_stats.rx_pkt_count = airstats->rxpackets;
    header->local_stats.rx_byte_count = airstats->rx_bytes;

    /* neighbours */
    if (_add_neighbours) {
      BRN_INFO("include Neighbours");
      header->flags |= INCLUDES_NEIGHBOURS;

      struct cooperative_channel_stats_body *body = (struct cooperative_channel_stats_body *)&header[1]; //body follows header
      struct neighbour_airtime_stats *nats = &(body->neighbour_stats);

      int n = 0;
      for (ChannelStats::SrcInfoTableIter iter = sit->begin(); iter.live(); iter++) {
         ChannelStats::SrcInfo *src = iter.value();
          EtherAddress ea = iter.key();

          src->calc_values();

          memcpy(nats[n]._etheraddr, ea.data(), 6);
          nats[n]._rx_pkt_count = src->_pkt_count;
          nats[n]._rx_byte_count = src->_byte_count;
          nats[n]._min_rssi = src->_min_rssi;
          nats[n]._max_rssi = src->_max_rssi;
          nats[n]._avg_rssi = src->_avg_rssi;
          nats[n]._std_rssi = src->_std_rssi;
          nats[n]._duration = src->_duration;
          n++;
        }
    }

    p = BRNProtocol::add_brn_header(p, BRN_PORT_CHANNELSTATSINFO, BRN_PORT_CHANNELSTATSINFO, 1, 0);

    BRN_INFO("Send Packet-Length: %d", p->length());

    output(0).push(p);
}

/*********************************************/
/************* Info from nodes****************/
/*********************************************/
void CooperativeChannelStats::push(int, Packet *p)
{
    BRN_DEBUG("void CooperativeChannelStats::push (int, Packet *p)");
    BRN_INFO("received Packet-Length: %d", p->length());

    click_ether *eh = (click_ether*) p->ether_header();
    EtherAddress src_ea = EtherAddress(eh->ether_shost);

    NodeChannelStats *ncs = _ncst.find(src_ea);

    if (ncs == NULL) {
      ncs = new NodeChannelStats(16);
      _ncst.insert(src_ea, ncs);
    }

    if (!ncs->insert(p)) p->kill();
}

NodeChannelStats *
CooperativeChannelStats::get_stats(EtherAddress *ea)
{
  if (_ncst.findp(*ea) == NULL) return NULL;

  return _ncst.find(*ea);
}

int
CooperativeChannelStats::get_neighbours_with_max_age(int age, Vector<EtherAddress> *ea_vec)
{
  int no_neighbours = 0;
  Timestamp now = Timestamp::now();

  for ( NodeChannelStatsMapIter nt_iter = _ncst.begin(); nt_iter != _ncst.end(); nt_iter++ ) {
    if ( (now - nt_iter.value()->_last_update).msecval() < age ) {
      ea_vec->push_back(nt_iter.key());
      no_neighbours++;
    }
  }
  return no_neighbours;
}

/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum
{
    H_STATS
};

String CooperativeChannelStats::stats_handler(int /*mode*/)
{
    StringAccum sa;

    sa << "<cooperativechannelstats" << " node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now();
    sa << "\" neighbours=\"" << _ncst.size() << "\" >\n";

    for(NodeChannelStatsMapIter iter = _ncst.begin(); iter.live(); iter++) {
      NodeChannelStats *ncs = iter.value();
      EtherAddress ea = iter.key();

      sa << "\t<neighbour addr=\"" << ea.unparse() << "\" record_size=\"" << ncs->_used << "\" last_id=\"";
      sa << ncs->get_last_stats()->stats_id << "\" >\n";

      NeighbourStatsMap *nsm = ncs->get_last_neighbour_map();
      struct local_airtime_stats *las = ncs->get_last_stats();


      uint32_t duration_sum = 0;
      for( NeighbourStatsMapIter iter_m = nsm->begin(); iter_m.live(); iter_m++) {
        struct neighbour_airtime_stats *n_nas = iter_m.value();
        duration_sum += (uint32_t)n_nas->_duration;
      }

      sa << "\t\t<last_record id=\"" << las->stats_id << "\" >\n";

      for( NeighbourStatsMapIter iter_m = nsm->begin(); iter_m.live(); iter_m++) {
        EtherAddress n_ea = iter_m.key();
        struct neighbour_airtime_stats *n_nas = iter_m.value();
        uint32_t duration_percent = (uint32_t)n_nas->_duration * (uint32_t)100 / duration_sum;

        sa << "\t\t\t<two_hop_neighbour addr=\"" << n_ea.unparse() << "\" rx_pkt=\"" << n_nas->_rx_pkt_count;
        sa << "\" rx_bytes=\"" << n_nas->_rx_byte_count << "\" min_rssi=\"" << (uint32_t)n_nas->_min_rssi;
        sa << "\" max_rssi=\"" << (uint32_t)n_nas->_max_rssi << "\" avg_rssi=\"" << (uint32_t)n_nas->_avg_rssi;
        sa << "\" std_rssi=\"" << (uint32_t)n_nas->_std_rssi << "\" duration=\"" << (uint32_t)n_nas->_duration << "\" duration_percent=\"" << duration_percent << "\" />\n";
      }

      sa << "\t\t</last_record>\n";

      sa << "\t</neighbour>\n";
    }

    sa << "</cooperativechannelstats>\n";

    return sa.take_string();
}

static String CooperativeChannelStats_read_param(Element *e, void *thunk)
{
    StringAccum sa;
    CooperativeChannelStats *td = (CooperativeChannelStats *) e;
    switch((uintptr_t) thunk)
    {
      case H_STATS:
        return (td->stats_handler((uintptr_t) thunk));
    }

    return String();
}

void CooperativeChannelStats::add_handlers()
{
    BRNElement::add_handlers();

    add_read_handler("stats", CooperativeChannelStats_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CooperativeChannelStats)
ELEMENT_REQUIRES(NodeChannelStats)
ELEMENT_MT_SAFE(CooperativeChannelStats)
