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
    _hn_del_timer(this),
    _hd_del_interval(1000)
{
  BRNElement::init();
}

HiddenNodeDetection::~HiddenNodeDetection()
{
}

int
HiddenNodeDetection::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "DEVICE", cpkP, cpElement, &_device,
                     "TIMEOUT", cpkP, cpInteger, &_hd_del_interval,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  return ret;
}

int
HiddenNodeDetection::initialize(ErrorHandler *)
{
  _last_data_dst = brn_etheraddress_broadcast;

  _hn_del_timer.initialize(this);
  _hn_del_timer.schedule_after_msec(_hd_del_interval);

  return 0;
}

void
HiddenNodeDetection::run_timer(Timer *)
{
  Timestamp now = Timestamp::now();

  for (NodeInfoTableIter iter = _nodeinfo_tab.begin(); iter.live(); iter++) {
    NodeInfo *node = iter.value();
    EtherAddress ea = iter.key();
    if ( (now - node->_last_notice_active).msecval() > _hd_del_interval ) {
      node->_neighbour = false;
      if ( (now - node->_last_notice_passive).msecval() > _hd_del_interval ) {
        node->_visible = false;
      }
    }
  }

  _hn_del_timer.reschedule_after_msec(_hd_del_interval);
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
    case WIFI_FC0_TYPE_MGT:
      src = EtherAddress(w->i_addr2);
      break;
    case WIFI_FC0_TYPE_CTL:
      src = brn_etheraddress_broadcast;
      break;
    case WIFI_FC0_TYPE_DATA:
      src = EtherAddress(w->i_addr2);
      break;
    default:
      src = EtherAddress(w->i_addr2);
      break;
  }
  EtherAddress dst = EtherAddress(w->i_addr1);

  /* General stuff */
  uint8_t _channel = BRNPacketAnno::channel_anno(p);
  if ( _device != NULL && _channel != 0 ) _device->setChannel(_channel);

  /* Handle TXFeedback */
  if ( ceh->flags & WIFI_EXTRA_TX ) {

    //(int) ceh->retries

  } else {   /* Handle Rx Packets */

    BRN_DEBUG("RX");
    NodeInfo *dst_ni = NULL;

    if ( (ceh->rate != 0) || (BrnWifi::getMCS(ceh,0) == 1)) { //has valid rate
      if ( (dst != brn_etheraddress_broadcast) && (*_device->getEtherAddress() != dst) ) {
        BRN_DEBUG("No broadcast");
        dst_ni = _nodeinfo_tab.find(dst);
        if ( ! dst_ni ) {
          BRN_DEBUG("New Dst");

          dst_ni = new NodeInfo();
          _nodeinfo_tab.insert(dst,dst_ni);
        }
        dst_ni->update_passive();
      }
      switch (type) {
        case WIFI_FC0_TYPE_MGT:
        case WIFI_FC0_TYPE_DATA:
          _last_data_dst = dst;
          _last_data_seq = le16_to_cpu(w->i_seq) >> WIFI_SEQ_SEQ_SHIFT;

          BRN_DEBUG("DATA");
          NodeInfo *src_ni = _nodeinfo_tab.find(src);
          if ( ! src_ni ) {
            BRN_DEBUG("New Src");

            src_ni = new NodeInfo();
            _nodeinfo_tab.insert(src,src_ni);
          }

          src_ni->update_active();
          src_ni->_neighbour = true;

          if (dst_ni) {
            BRN_DEBUG("Dst");

	    Timestamp ts = Timestamp(0,0)
//            dst_ni->add_link(src, src_ni, &ts);
	    ts = Timestamp::now()
            src_ni->add_link(dst, dst_ni, &ts);
          }

          break;
//        case WIFI_FC0_TYPE_CTL:
//          break;
      }
            
      BRN_DEBUG("Fin");

    } else { //RX with rate = 0

    }
  }

  output(port).push(p);
}


/**************************************************************************************************************/
/********************************************** H A N D L E R *************************************************/
/**************************************************************************************************************/

enum {H_STATS};

String
HiddenNodeDetection::stats_handler(int mode)
{
  StringAccum sa;

  switch (mode) {
    case H_STATS:

      sa << "<channelerrordetection node=\"";

      if ( _device )
        sa << _device->getEtherAddress()->unparse();
      else
        sa << BRN_NODE_NAME;

      sa << "\" >\n\t<neighbour_nodes>\n";

      for (NodeInfoTableIter iter = _nodeinfo_tab.begin(); iter.live(); iter++) {
        NodeInfo *node = iter.value();
        EtherAddress ea = iter.key();
        if ( node->_neighbour ) {
          sa << "\t\t<node addr=\"" << ea.unparse() << "\" last_packet=\"" << node->_last_notice_active.unparse() << "\" hidden_nodes=\"" << node->_links_to.size() << "\" >\n";
          for (NodeInfoTableIter link_iter = node->_links_to.begin(); link_iter.live(); link_iter++) {
            NodeInfo *l_node = link_iter.value();
            EtherAddress l_ea = link_iter.key();
            if ( ! l_node->_neighbour && l_node->_visible ) {
              Timestamp *ts = node->_links_usage.findp(l_ea);
              sa << "\t\t\t<hidden_node addr=\"" << l_ea.unparse() << "\" last_packet=\"" << ts->unparse() << "\" />\n";
            }
          }
          sa << "\t\t</node>\n";
        }
      }

      sa << "\t</neighbour_nodes>\n\t<hidden_nodes>\n";

      for (NodeInfoTableIter iter = _nodeinfo_tab.begin(); iter.live(); iter++) {
        NodeInfo *node = iter.value();
        EtherAddress ea = iter.key();
        if ( ! node->_neighbour && node->_visible ) {
          sa << "\t\t<node addr=\"" << ea.unparse() << "\" last_packet=\"" << node->_last_notice_passive.unparse() << "\" last_active=\"" << node->_last_notice_active.unparse() << "\" />\n";
        }
      }

      sa << "\t</hidden_nodes>\n</channelerrordetection>\n";

  }
  return sa.take_string();
}

static String 
HiddenNodeDetection_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  HiddenNodeDetection *td = (HiddenNodeDetection *)e;
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
ELEMENT_MT_SAFE(HiddenNodeDetection)
