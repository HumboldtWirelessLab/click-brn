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
#include <clicknet/wifi.h>

#include <elements/brn/brn2.h>
#include <elements/wifi/bitrate.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>

#include "hiddennodedetection.hh"

CLICK_DECLS

HiddenNodeDetection::HiddenNodeDetection():
    _device(NULL),
    _lt(NULL),
    _cocst(NULL),
    _hn_del_timer(this),
    _hnd_del_interval(1000),
    _hn_link_del_interval(0),
    _last_data_seq(0)
{
  BRNElement::init();
}

HiddenNodeDetection::~HiddenNodeDetection()
{
}

int
HiddenNodeDetection::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _cocst_string = String("");
  _lt_string = String("");

  int ret = cp_va_kparse(conf, this, errh,
                     "DEVICE", cpkP, cpElement, &_device,
                     "TIMEOUT", cpkP, cpInteger, &_hnd_del_interval,
                     "LINKTIMEOUT", cpkP, cpInteger, &_hn_link_del_interval,
                     "LINKTABLE", cpkP, cpElement, &_lt,
                     "LINKTABLEPATH", cpkP, cpString, &_lt_string,
                     "COOPCHANNELSTATS", cpkP, cpElement, &_cocst,
                     "COOPCHANNELSTATSPATH", cpkP, cpString, &_cocst_string,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  if (_hn_link_del_interval < _hnd_del_interval) _hn_link_del_interval = _hnd_del_interval;

  return ret;
}

int
HiddenNodeDetection::initialize(ErrorHandler *errh)
{
  _nodeinfo_tab.clear();

  _last_data_dst = brn_etheraddress_broadcast;

  _hn_del_timer.initialize(this);
  _hn_del_timer.schedule_after_msec(_hnd_del_interval);

  if ((_cocst == NULL) && (_cocst_string != "")) {
    Element *e = cp_element(_cocst_string, this, errh);
    _cocst = reinterpret_cast<CooperativeChannelStats*>(e);
  }

  if ((_lt == NULL) && (_lt_string != "")) {
    Element *e = cp_element(_lt_string, this, errh);
    _lt = reinterpret_cast<Brn2LinkTable*>(e);
  }

  return 0;
}

void
HiddenNodeDetection::run_timer(Timer *)
{
  Timestamp now = Timestamp::now();

  //reschedule first, so that long computation time has no impact on interval length
  _hn_del_timer.schedule_after_msec(_hnd_del_interval);

  if ( _cocst != NULL ) handle_coop_channelstats();
  if ( _lt != NULL ) handle_linktable();

  for (NodeInfoTableIter iter = _nodeinfo_tab.begin(); iter.live(); ++iter) {
    NodeInfo *node = iter.value();
    EtherAddress node_ea = iter.key();

    BRN_DEBUG("Check Node: %s active: %d passive: %d Timeout: %d",node_ea.unparse().c_str(),
                                                                (int32_t)(now - node->_last_notice_active).msecval(),
                                                                (int32_t)(now - node->_last_notice_passive).msecval(),
                                                                _hnd_del_interval);

    /* check age of node! Is neighbour? does it still exists? */
    if ( (now - node->_last_notice_active).msecval() > _hnd_del_interval ) {
      node->_neighbour = false;
      if ( (now - node->_last_notice_passive).msecval() > _hnd_del_interval )
        node->_exists = false;
    }

    /* check links of existing nodes. Remove if too old!  */
    if ( node->_exists == true ) {
      for ( EtherTimestampMapIter nt_iter = node->_last_link_usage.begin(); nt_iter != node->_last_link_usage.end(); ++nt_iter ) {
        Timestamp last_use = nt_iter.value();
        EtherAddress neighbour_node_ea = nt_iter.key();
        if ( (now - last_use).msecval() > _hn_link_del_interval ) {         //link is to old, remove rev and fwd
          NodeInfo *neighbour_node = _nodeinfo_tab.find(neighbour_node_ea); //get nodeinfo from rev_node
          neighbour_node->_last_link_usage.remove(node_ea);                 //remove rev
          neighbour_node->_links_to.remove(node_ea);
          node->_last_link_usage.remove(neighbour_node_ea);                 //remove fwd
          node->_links_to.remove(neighbour_node_ea);
        }
      }
    }
  }

}

/*********************************************/
/************* RX BASED STATS ****************/
/*********************************************/
void
HiddenNodeDetection::push(int port, Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  struct click_wifi *w = (struct click_wifi *) p->data();
  int type = w->i_fc[0] & WIFI_FC0_TYPE_MASK;

  EtherAddress src;
  switch (type) {
    case WIFI_FC0_TYPE_CTL:
      switch (w->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {
        case WIFI_FC0_SUBTYPE_RTS:
          src = EtherAddress(w->i_addr2);
          break;
        default:
          src = brn_etheraddress_broadcast;
      }
      break;
    default: //MGT & DATA
      src = EtherAddress(w->i_addr2);
      break;
  }

  EtherAddress dst = EtherAddress(w->i_addr1);

  /* Handle TXFeedback */
  if ( ceh->flags & WIFI_EXTRA_TX ) {
    //TODO: success ? then add dst as neighbour
    //(int) ceh->retries

  } else {   /* Handle Rx Packets */

    NodeInfo *src_ni = _nodeinfo_tab.find(src);

    if ( ! src_ni ) {
      BRN_DEBUG("New Src");

      src_ni = new NodeInfo();
      _nodeinfo_tab.insert(src, src_ni);
    }

    src_ni->update_active();
    src_ni->_neighbour = true;

    //BRN_ERROR("RX: %s -> %s",src.unparse().c_str(), dst.unparse().c_str());

    if ( (ceh->rate != 0) || (BrnWifi::getMCS(ceh,0) == 1)) { //has valid rate

      if ( (dst != brn_etheraddress_broadcast) && (*_device->getEtherAddress() != dst) ) {
        BRN_DEBUG("No broadcast");
        NodeInfo *dst_ni = _nodeinfo_tab.find(dst);
        if ( ! dst_ni ) {
          BRN_DEBUG("New Dst");

          dst_ni = new NodeInfo();
          dst_ni->_neighbour = false;
          _nodeinfo_tab.insert(dst,dst_ni);
        }
        dst_ni->update_passive();


        switch (type) {
            case WIFI_FC0_TYPE_MGT:
            case WIFI_FC0_TYPE_DATA:
            case WIFI_FC0_TYPE_CTL:
                _last_data_dst = dst;
                _last_data_seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;

                BRN_DEBUG("DATA");
                BRN_DEBUG("Dst");

                Timestamp ts = Timestamp::now();
                dst_ni->add_link(src, src_ni, &ts);
                src_ni->add_link(dst, dst_ni, &ts);

                break;
        }
      }

      BRN_DEBUG("Fin");

    } else { //RX with rate = 0

    }
  }

  output(port).push(p);
}

/**************************************************************************************************************/
/************** H I D D E N N O D E D E T E C T I O N   W I T H   C O O P E R A T I O N C S T *****************/
/**************************************************************************************************************/
void
HiddenNodeDetection::handle_coop_channelstats()
{
  if ( _cocst != NULL ) {
    BRN_DEBUG("Use CoopCst");
    Timestamp now = Timestamp::now();
    NodeChannelStatsMap *nodeCstMap = _cocst->get_stats_map();
    Vector<EtherAddress> nb_ea_vec;

    _cocst->get_neighbours_with_max_age(_hnd_del_interval, &nb_ea_vec);
    BRN_DEBUG("Handle %d nbs",nb_ea_vec.size());

    for (Vector<EtherAddress>::const_iterator ea_iter = nb_ea_vec.begin(); ea_iter != nb_ea_vec.end(); ++ea_iter) {
      BRN_DEBUG("Handle %s",ea_iter->unparse().c_str());

      EtherAddress src = *ea_iter;
      NodeChannelStats *nb_cst = nodeCstMap->find(src);
      NodeInfo *src_ni = _nodeinfo_tab.find(src);

      assert(src_ni != NULL); //src_ni sould be there due to push

      src_ni->update_active();
      src_ni->_neighbour = true;

      BRN_DEBUG("NB has %d nbs",nb_cst->_last_neighbor_update.size());
      for ( EtherTimestampMapIter nb_time_iter = nb_cst->_last_neighbor_update.begin(); nb_time_iter.live(); ++nb_time_iter ) {

        BRN_DEBUG("NB-nb: %s age %d",nb_time_iter.key().unparse().c_str(), (now-nb_time_iter.value()).msecval());
        if ((now-nb_time_iter.value()).msecval() < _hnd_del_interval) {

          EtherAddress dst = nb_time_iter.key();

          if ((dst != brn_etheraddress_broadcast) && (*_device->getEtherAddress() != dst)) {
            NodeInfo *dst_ni = _nodeinfo_tab.find(dst);
            if ( ! dst_ni ) {
              BRN_DEBUG("New Dst");

              dst_ni = new NodeInfo();
              dst_ni->_neighbour = false;
              _nodeinfo_tab.insert(dst,dst_ni);
            }

            dst_ni->update_passive();
            dst_ni->add_link(src, src_ni, &now);
            src_ni->add_link(dst, dst_ni, &now);
          }
        }
      }
    }

    nb_ea_vec.clear();
  }
}

/**************************************************************************************************************/
/*************** H I D D E N N O D E D E T E C T I O N   W I T H   L I N K T A B L E **************************/
/**************************************************************************************************************/
void
HiddenNodeDetection::handle_linktable()
{
  Timestamp now = Timestamp::now();

  Vector<EtherAddress> l_neighbors;
  Vector<EtherAddress> n_neighbors;

  _lt->get_local_neighbors(l_neighbors);

  /* Add one-hop-neighbours */
  for (Vector<EtherAddress>::const_iterator ea_iter = l_neighbors.begin(); ea_iter != l_neighbors.end(); ++ea_iter) {
      BRN_DEBUG("Handle %s",ea_iter->unparse().c_str());

      EtherAddress src = *ea_iter;

      NodeInfo *src_ni = _nodeinfo_tab.find(src);
      if ( ! src_ni ) {
        BRN_DEBUG("New src");

        src_ni = new NodeInfo();
        _nodeinfo_tab.insert(src,src_ni);
      }

      src_ni->update_active();
      src_ni->_neighbour = true;
  }

  /* add neighbours of neighbours if they are not my neighbours */
  n_neighbors.clear();

  for (Vector<EtherAddress>::const_iterator ea_iter = l_neighbors.begin(); ea_iter != l_neighbors.end(); ++ea_iter) {
    BRN_DEBUG("Handle %s",ea_iter->unparse().c_str());

    EtherAddress src = *ea_iter;
    NodeInfo *src_ni = _nodeinfo_tab.find(src);

    _lt->get_neighbors(src, n_neighbors);

    for (Vector<EtherAddress>::const_iterator n_ea_iter = n_neighbors.begin(); n_ea_iter != n_neighbors.end(); ++n_ea_iter) {
      BRN_DEBUG("Handle %s",n_ea_iter->unparse().c_str());

      EtherAddress dst = *n_ea_iter;

      if (*_device->getEtherAddress() != dst) {
            NodeInfo *dst_ni = _nodeinfo_tab.find(dst);
            if ( ! dst_ni ) {
              BRN_DEBUG("New Dst");

              dst_ni = new NodeInfo();
              dst_ni->_neighbour = false;
              _nodeinfo_tab.insert(dst,dst_ni);
            }

            dst_ni->update_passive();
            dst_ni->add_link(src, src_ni, &now);
            if ( dst != brn_etheraddress_broadcast ) src_ni->add_link(dst, dst_ni, &now);
      }
    }
    n_neighbors.clear();
  }
}

/**************************************************************************************************************/
/********************************************* F U N C T I O N S **********************************************/
/**************************************************************************************************************/

int
HiddenNodeDetection::count_hidden_neighbours(const EtherAddress &ea)
{
  if ( _nodeinfo_tab.findp(ea) == NULL ) return 0;

  int count_hn = 0;

  NodeInfo* ni = _nodeinfo_tab.find(ea);

  //TODO: use neighbour flag
  for ( EtherTimestampMapIter nt_iter = ni->_last_link_usage.begin(); nt_iter != ni->_last_link_usage.end(); ++nt_iter ) {
    NodeInfo *node = _nodeinfo_tab.find(nt_iter.key());
    if ((!node->_neighbour) && node->_exists) count_hn++;
  }

  return count_hn;
}

int
HiddenNodeDetection::get_hidden_neighbours(const EtherAddress &ea, Vector<EtherAddress> *hns)
{
  hns->clear();

  if ( _nodeinfo_tab.findp(ea) == NULL ) return 0;

  NodeInfo* ni = _nodeinfo_tab.find(ea);

  for ( EtherTimestampMapIter nt_iter = ni->_last_link_usage.begin(); nt_iter != ni->_last_link_usage.end(); ++nt_iter ) {
    NodeInfo *node = _nodeinfo_tab.find(nt_iter.key());
    if ((!node->_neighbour) && node->_exists) hns->push_back(nt_iter.key());
  }

  return hns->size();
}


/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum {H_STATS};

String
HiddenNodeDetection::stats_handler(int mode)
{
  StringAccum sa;
  sa.reserve(4096);

  switch (mode) {
    case H_STATS:

      sa << "<hiddennodedetection node=\"";

      if ( _device )
        sa << _device->getEtherAddress()->unparse();
      else
        sa << BRN_NODE_NAME;

      sa << "\" time=\"" << Timestamp::now().unparse() << "\" >\n\t<neighbour_nodes>\n";

      for (NodeInfoTableIter iter = _nodeinfo_tab.begin(); iter.live(); ++iter) {
        NodeInfo *node = iter.value();
        EtherAddress ea = iter.key();
        if ( node->_neighbour ) {
          sa << "\t\t<node addr=\"" << ea.unparse() << "\" last_packet=\"" << node->_last_notice_active.unparse();
          sa << "\" hidden_nodes=\"" << count_hidden_neighbours(ea) << "\" >\n";
          for (NodeInfoTableIter link_iter = node->_links_to.begin(); link_iter.live(); ++link_iter) {
            NodeInfo *l_node = link_iter.value();
            EtherAddress l_ea = link_iter.key();
            if ( (!l_node->_neighbour) && l_node->_exists ) {
              Timestamp *ts = node->_last_link_usage.findp(l_ea);
              sa << "\t\t\t<hidden_node addr=\"" << l_ea.unparse() << "\" last_link_usage=\"" << ts->unparse() << "\" />\n";
            }
          }
          sa << "\t\t</node>\n";
        }
      }

      sa << "\t</neighbour_nodes>\n\t<hidden_nodes>\n";

      for (NodeInfoTableIter iter = _nodeinfo_tab.begin(); iter.live(); ++iter) {
        NodeInfo *node = iter.value();
        EtherAddress ea = iter.key();
        if ( ! node->_neighbour && node->_exists ) {
          sa << "\t\t<node addr=\"" << ea.unparse() << "\" last_passive=\"" << node->_last_notice_passive.unparse();
          sa << "\" last_active=\"" << node->_last_notice_active.unparse() << "\" />\n";
        }
      }

      sa << "\t</hidden_nodes>\n</hiddennodedetection>\n";
  }

  return sa.take_string();
}

static String 
HiddenNodeDetection_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  HiddenNodeDetection *td = reinterpret_cast<HiddenNodeDetection *>(e);
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->stats_handler((uintptr_t) thunk);
      break;
  }

  return String();
}

void
HiddenNodeDetection::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", HiddenNodeDetection_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(HiddenNodeDetection)
