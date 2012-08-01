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
    BRN_DEBUG("CooperativeChannelStats::CooperativeChannelStats():");
    BRNElement::init();
}

CooperativeChannelStats::~CooperativeChannelStats()
{
    BRN_DEBUG("CooperativeChannelStats::~CooperativeChannelStats()");
}

int CooperativeChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)
{    
    BRN_DEBUG("int CooperativeChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)");
    int ret = cp_va_kparse(conf, this, errh,
                     "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
                     "INTERVAL", cpkP, cpInteger, &_interval,
                     "NEIGHBOURS", cpkP, cpBool, &_add_neighbours,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

    return ret;
}

int CooperativeChannelStats::initialize(ErrorHandler *errh)
{
    BRN_DEBUG("int CooperativeChannelStats::initialize(ErrorHandler *)");
    click_srandom(_cst->_device->getEtherAddress()->hashcode());
    _msg_timer.initialize(this);
    
    if ( _interval > 0 )
    {    
        _msg_timer.schedule_after_msec((click_random() % (_interval-100)) + 100);
    }

    return 0;
}

void CooperativeChannelStats::run_timer(Timer *timer)
{
    BRN_DEBUG("void CooperativeChannelStats::run_timer(Timer *)");
    _msg_timer.schedule_after_msec(_interval);
    send_message();
}

inline WritablePacket * create_new_packet(struct cooperative_channel_stats_header ccsh, struct neighbour_airtime_stats nats_arr[])
{
    WritablePacket *p = Packet::make
            (
            128,
            NULL,
            sizeof(struct cooperative_channel_stats_header) + sizeof(neighbour_airtime_stats) * ccsh.no_neighbours,
            32
            );
    
    unsigned char *data = p->data();
    memcpy(data, &ccsh, sizeof(struct cooperative_channel_stats_header));

    if (ccsh.no_neighbours > 0)
    {
        int pointer = sizeof(struct cooperative_channel_stats_header);

        for (uint16_t i = 0; i < ccsh.no_neighbours; i++)
        {    
            memcpy(&data[pointer], &nats_arr[i], sizeof(neighbour_airtime_stats));
            pointer += sizeof(neighbour_airtime_stats);
        }
    }
    
    return p;
}

void CooperativeChannelStats::send_message()
{
    BRN_DEBUG("void CooperativeChannelStats::send_message()");
    ChannelStats::SrcInfoTable *sit = _cst->get_latest_stats_neighbours();
     
    struct cooperative_channel_stats_header ccsh;
    ccsh.endianess = ENDIANESS_TEST;
    ccsh.flags = 0;
    
    struct neighbour_airtime_stats nats_arr[sit->size()];
    uint8_t non = 0; //number of available neighbours
    
    if (_add_neighbours)
    {    
        BRN_DEBUG("include Neighbours");
        ccsh.flags |= INCLUDES_NEIGHBOURS;
        
        for (ChannelStats::SrcInfoTableIter iter = sit->begin(); iter.live(); iter++)
        {            
            ChannelStats::SrcInfo src = iter.value();
            EtherAddress ea = iter.key();
            struct neighbour_airtime_stats nats;
            src.get_airtime_stats(&ea, &nats);
            nats_arr[non++] = nats;
        }
    }
    
    BRN_DEBUG("Number of Neighbours: %d", non);
    ccsh.no_neighbours = non;

    WritablePacket *p = create_new_packet(ccsh, nats_arr);
    
    #warning check stettings of brn_headers
    p = BRNProtocol::add_brn_header(p, BRN_PORT_CHANNELSTATSINFO, BRN_PORT_CHANNELSTATSINFO, 255, 0);
    BRN_DEBUG("Send Packet-Length: %d", p->length());
    
    output(0).push(p);
}

/*********************************************/
/************* Info from nodes****************/
/*********************************************/
void CooperativeChannelStats::push(int, Packet *p)
{    
    BRN_DEBUG("void CooperativeChannelStats::push(int, Packet *p)");
    BRN_DEBUG("received Packet-Length: %d", p->length());
    
    click_ether *eh = (click_ether*)p->ether_header();
    EtherAddress src_ea = EtherAddress(eh->ether_shost);
    
    if (_ncst.find(src_ea) == NULL ) _ncst.insert(src_ea, new NodeChannelStats(src_ea));
    NodeChannelStats *ncs = _ncst.find(src_ea);
    
    const unsigned char *data = p->data();
    struct cooperative_channel_stats_header ccsh; 
    
    memcpy(&ccsh, data, sizeof(cooperative_channel_stats_header));
    
    BRN_DEBUG("FLAGG: %d", ccsh.flags);
    BRN_DEBUG("Neighbours: %d", ccsh.no_neighbours);
    
    if (ccsh.flags > 0)
    {
        struct neighbour_airtime_stats nats_arr[ccsh.no_neighbours];
        memcpy(&nats_arr, &data[sizeof(cooperative_channel_stats_header)], sizeof(neighbour_airtime_stats) * ccsh.no_neighbours);
        
        uint8_t i = 0;
        for (i = 0; i < ccsh.no_neighbours; i++)
        {            
            EtherAddress temp_ea = EtherAddress(nats_arr[i]._etheraddr);
            
            BRN_DEBUG("RECEIVED: Address %s: bytes: %d, packets: %d, duration: %d, avg_rssi: %d", 
                    temp_ea.unparse().c_str(),
                    nats_arr[i]._byte_count,
                    nats_arr[i]._pkt_count,
                    nats_arr[i]._duration,
                    nats_arr[i]._avg_rssi);
            
            ncs->add_neighbour_stats(temp_ea, nats_arr[i]);
        }
    }
    
    p->kill();
}

HashMap<EtherAddress, struct neighbour_airtime_stats*> CooperativeChannelStats::get_stats(const EtherAddress &ea)
{
    
    BRN_DEBUG("HashMap<EtherAddress, struct neighbour_airtime_stats*> CooperativeChannelStats::get_stats(EtherAddress *ea)");
    if (_ncst.find(ea) != NULL)
    {
        BRN_DEBUG("FOUND EA: %s", ea.unparse().c_str());
        return _ncst.find(ea)->_n_stats;
        
    } else
    {
        HashMap<EtherAddress, struct neighbour_airtime_stats*> hm;
        BRN_DEBUG("DID NOT FOUND EA: %s", ea.unparse().c_str());
        
        return hm;
    }
}

/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum { H_STATS };

String CooperativeChannelStats::stats_handler(int mode)
{    
    StringAccum sa;
    
    sa << "<cooperativechannelstats" << " node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now();
    sa << "\" neighbours=\"" << _ncst.size() << "\" >\n";
    
    for (NodeChannelStatsTableIter iter = _ncst.begin(); iter.live(); iter++)
    {        
        NodeChannelStats *ncs = iter.value();
        EtherAddress ea = iter.key();
        
        sa << "\t<node address=\"" << ncs->node.unparse() << "\" />\n";
    }
    
    sa << "</cooperativechannelstats>\n";
    
    return sa.take_string();
}

static String
CooperativeChannelStats_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  CooperativeChannelStats *td = (CooperativeChannelStats *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->stats_handler((uintptr_t) thunk);
      break;
  }

  return String();
}

void
CooperativeChannelStats::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", CooperativeChannelStats_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CooperativeChannelStats)
ELEMENT_REQUIRES(NodeChannelStats)
ELEMENT_REQUIRES(CooperativeStatsCircularBuffer)
ELEMENT_MT_SAFE(CooperativeChannelStats)
