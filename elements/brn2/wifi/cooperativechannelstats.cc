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

#include <click/config.h>
#include <click/straccum.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>
#include <click/error.hh>
#include <click/userutils.hh>
#include <click/timer.hh>

#include <elements/brn2/brn2.h>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/routing/identity/brn2_device.hh"

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
}

int
CooperativeChannelStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "CHANNELSTATS", cpkP+cpkM, cpElement, &_cst,
                     "INTERVAL", cpkP, cpInteger, &_interval,
                     "NEIGHBOURS", cpkP, cpBool, &_add_neighbours,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  return ret;
}

int CooperativeChannelStats::initialize(ErrorHandler *) {
    
    click_srandom(_cst->_device->getEtherAddress()->hashcode());
    _msg_timer.initialize(this);
    
    if ( _interval > 0 ) {
        
        _msg_timer.schedule_after_msec((click_random() % (_interval-100)) + 100);
    }

    return 0;
}

void CooperativeChannelStats::run_timer(Timer *) {

    _msg_timer.schedule_after_msec(_interval);
    send_message();
}


void CooperativeChannelStats::send_message() {
    
    BRN_DEBUG("Send message...");
    //struct neighbour_airtime_stats *nats;
    ChannelStats::SrcInfoTable *sit = _cst->get_latest_stats_neighbours();
    
    
    //struct airtime_stats *last_ats = _cst->get_latest_stats(); //ats -> AirTimeStats

    // if ( last_ats == NULL ) return;
    
    WritablePacket *p = Packet::make(128 /*headroom*/, NULL,
                //sizeof(struct cooperative_channel_stats_header) + sizeof(struct airtime_stats),
                sizeof(struct cooperative_channel_stats_header) + sizeof(struct neighbour_airtime_stats) * sit->size() /* all neighbours */,
                32); //alignment

    struct cooperative_channel_stats_header *ccsh = (struct cooperative_channel_stats_header*)p->data();
    ccsh->endianess = ENDIANESS_TEST;
    ccsh->flags = 0;

//    struct airtime_stats *ats = (struct airtime_stats *)&(ccsh[1]);
    
//    memcpy( ats, last_ats, sizeof(struct airtime_stats));

    uint8_t non = 0; //number of available neighbours
    struct neighbour_airtime_stats *all_nats = (struct neighbour_airtime_stats *)&(ccsh[1]);
    
    if ( _add_neighbours ) {
        
        BRN_DEBUG("include Neighbours");
        ccsh->flags |= INCLUDES_NEIGHBOURS;
        
        uint8_t it = 0;
        for (ChannelStats::SrcInfoTableIter iter = sit->begin(); iter.live(); iter++) {
            
            ChannelStats::SrcInfo src = iter.value();
            EtherAddress ea = iter.key();
            struct neighbour_airtime_stats nats;
            src.get_airtime_stats(&ea, &nats);
            non++;
            memcpy(all_nats, &nats, sizeof(struct neighbour_airtime_stats));
        }
    }
    
    BRN_DEBUG("Number of Neighbours: %d", non);
    ccsh->no_neighbours = non;

#warning check stettings of brn_headers
  p = BRNProtocol::add_brn_header(p, BRN_PORT_CHANNELSTATSINFO, BRN_PORT_CHANNELSTATSINFO, 255, 0);

  output(0).push(p);
  BRN_DEBUG(".....Send Message");
}

/*********************************************/
/************* Info from nodes****************/
/*********************************************/
void
CooperativeChannelStats::push(int port, Packet *p)
{
    BRN_DEBUG("Push....");
    click_ether *eh = (click_ether*)p->ether_header();
    EtherAddress src_ea = EtherAddress(eh->ether_shost);

    if ( ncst.find(src_ea) == NULL ) ncst.insert(src_ea, new NodeChannelStats(src_ea));
    NodeChannelStats *ncs = ncst.find(src_ea);
    struct cooperative_channel_stats_header *ccsh = (struct cooperative_channel_stats_header*)p->data();
    struct airtime_stats *ats = (struct airtime_stats *)&(ccsh[1]);
    ncs->set_stats(ats, ccsh->endianess );
    if (ncs->_n_stats.find(src_ea) != NULL) {
        BRN_DEBUG("%s RSSI: %d", src_ea.unparse().c_str(), ncs->_n_stats.find(src_ea)->_avg_rssi);
    }
    BRN_DEBUG("CSSH flags: %d", ccsh->flags);
    BRN_DEBUG("%s Neighbour Number: %d", src_ea.unparse().c_str(), ccsh->no_neighbours);
 //   if (ccsh != NULL)
    p->kill();
//    else
//        output(port).push(p);
}

/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum { H_STATS };

String
CooperativeChannelStats::stats_handler(int mode)
{
  StringAccum sa;

  sa << "<cooperativechannelstats" << " node=\"" << BRN_NODE_NAME << "\" time=\"" << Timestamp::now();
  sa << "\" neighbours=\"" << ncst.size() << "\" >\n";

  for (NodeChannelStatsTableIter iter = ncst.begin(); iter.live(); iter++) {
    NodeChannelStats *ncs = iter.value();
    EtherAddress ea = iter.key();

    sa << "\t<node address=\"" << ncs->node.unparse() << "\" />\n";

    /*for (NodeChannelStatsTableIter iter = ncst.begin(); iter.live(); iter++) {
      NodeChannelStats *ncs = iter.value();
      EtherAddress ea = iter.key();

      sa << "\t<neighbour address=\"" << ncs->node.unparse() << "\" />\n";
    }*/
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
ELEMENT_MT_SAFE(CooperativeChannelStats)
