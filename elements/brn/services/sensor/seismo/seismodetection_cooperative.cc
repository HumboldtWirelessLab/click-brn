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
 * seismo.{cc,hh}
 */

#include <click/config.h>
#include <click/confparse.hh>

#include <stdio.h>
#include <unistd.h>
#include <click/userutils.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "seismo_reporting.hh"
#include "seismodetection_cooperative.hh"

CLICK_DECLS

SeismoDetectionCooperative::SeismoDetectionCooperative():
  _max_alarm(SEISMO_REPORT_MAX_ALARM_COUNT),
  _merge_range(2000),
  _group_alarm_id(0),
  _fract_threshold(50)
{
  BRNElement::init();
}

SeismoDetectionCooperative::~SeismoDetectionCooperative()
{
}

void *
SeismoDetectionCooperative::cast(const char *n)
{
  if (strcmp(n, "SeismoDetectionCooperative") == 0)
    return (SeismoDetectionCooperative *) this;
  else if (strcmp(n, "SeismoDetectionAlgorithm") == 0)
    return (SeismoDetectionAlgorithm *) this;
  else
    return 0;
}

int
SeismoDetectionCooperative::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ALGORITHMS", cpkP+cpkM, cpString, &_algo_string,
      "MERGERANGE", cpkP, cpInteger, &_merge_range,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _local_addr = EtherAddress();
  _group_addr = EtherAddress::make_broadcast();

  uint8_t alarm_addr[] = {255,255,255,255,255,254};
  _alarm_addr = EtherAddress(alarm_addr);

  return 0;
}

int
SeismoDetectionCooperative::initialize(ErrorHandler *errh)
{
  Vector<String> algo_vec;
  cp_spacevec(_algo_string, algo_vec);

  for (int i = 0; i < algo_vec.size(); i++) {
    Element *new_element = cp_element(algo_vec[i] , this, errh, NULL);
    if ( new_element != NULL ) {
      //click_chatter("El-Name: %s", new_element->class_name());
      SeismoDetectionAlgorithm *sda =
        (SeismoDetectionAlgorithm *)new_element->cast("SeismoDetectionAlgorithm");
      if ( sda != NULL ) {
        _sdal.push_back(sda);
      }
    }
  }

  _last_packet = Timestamp::now();

  _salm.insert(_local_addr,new SeismoAlarmList());
  _salm.insert(_group_addr,new SeismoAlarmList());
  _salm.insert(_alarm_addr,new SeismoAlarmList());

  return 0;
}

void
SeismoDetectionCooperative::push(int /*port*/, Packet *p)
{
  BRN_DEBUG("Push");
  struct click_seismodetection_coop_header *sdc_header =
                                       (struct click_seismodetection_coop_header *)p->data();


  click_ether *annotated_ether = (click_ether *)p->ether_header();
  EtherAddress srcEther = EtherAddress(annotated_ether->ether_shost);

  SeismoAlarmList *node_sal = _salm.find(srcEther);

  if ( node_sal == NULL ) {
    node_sal = new SeismoAlarmList();
    _salm.insert(srcEther,node_sal);
  }

  struct click_seismodetection_alarm_info *ai =
               (struct click_seismodetection_alarm_info *)&(sdc_header[1]);

  int no_new_alarm = 0;

  for ( int i = 0; i < sdc_header->no_alarms; i++) {
    if ( (node_sal->size() == 0) ||
         (htons(ai[i]._id) > (*node_sal)[node_sal->size()-1]->_id) ||
         (htons(ai[i]._id) < (*node_sal)[0]->_id) ) {
      SeismoAlarm *sa = new SeismoAlarm();
      sa->_start = Timestamp::make_msec(ntohl(ai[i]._start));
      sa->_end = Timestamp::make_msec(ntohl(ai[i]._end));
      sa->_id = ntohs(ai[i]._id);
      node_sal->push_back(sa);
      no_new_alarm++;

      BRN_DEBUG("New Alarm: %d",sa->_id);

      if ( node_sal->size() > _max_alarm ) {
        if ( (*node_sal)[0]->_detection_info != NULL )
          //delete (*node_sal)[0]->_detection_info;

        delete (*node_sal)[0];
        node_sal->erase(node_sal->begin());
      }
    }
  }

  if ( (no_new_alarm > 0) && (_salm.size() > 2) )
    get_cooperative_alarms();
}

void
SeismoDetectionCooperative::update(SrcInfo */*si*/, uint32_t /*next_new_block*/)
{
  BRN_DEBUG("update");
  if ( (Timestamp::now() - _last_packet).msecval() > 1000 ) {
    BRN_DEBUG("Trigger");

    SeismoAlarmList *sal = _sdal[0]->get_alarm();
    SeismoAlarmList *local_sal = _salm.find(_local_addr);

    if ( sal->size() > 0 ) {
      BRN_DEBUG("Alg %d has %d alarms",0,sal->size());
      WritablePacket *new_packet = NULL;

      int psize = sizeof(struct click_seismodetection_coop_header) +
                  sal->size() * sizeof(struct click_seismodetection_alarm_info);

      new_packet = WritablePacket::make( 256, NULL, psize, 32);

      struct click_seismodetection_coop_header *sdc_header = (struct click_seismodetection_coop_header *)new_packet->data();

      sdc_header->flags = 0;
      sdc_header->no_alarms = (uint8_t)sal->size();

      struct click_seismodetection_alarm_info *sai = (struct click_seismodetection_alarm_info*)&sdc_header[1];

      int no_new_alarm = 0;

      for ( int i = 0; i < sal->size(); i++) {
        //copy alarm info to packet
        SeismoAlarm *s = (*sal)[i];
        BRN_DEBUG("Send alarm: %d", (uint32_t)s->_id);
        sai[i]._start = htonl(s->_start.msecval());
        sai[i]._end = htonl(s->_end.msecval());
        sai[i]._id = htons(s->_id);

        //make local copy of alarm info for cooperative alg
        if ( (local_sal->size() == 0) ||
         (s->_id > (*local_sal)[local_sal->size()-1]->_id) ||
         (s->_id < (*local_sal)[0]->_id) ) {

          SeismoAlarm *sa = new SeismoAlarm();
          sa->_start = s->_start;
          sa->_end = s->_end;
          sa->_id = s->_id;
          local_sal->push_back(sa);

          no_new_alarm++;

          if ( local_sal->size() > _max_alarm ) {
            if ( (*local_sal)[0]->_detection_info != NULL )
            //delete (*node_sal)[0]->_detection_info;

            delete (*local_sal)[0];
            local_sal->erase(local_sal->begin());
          }
        }
      }

      WritablePacket *p_out = BRNProtocol::add_brn_header(new_packet, BRN_PORT_SEISMO_COOPERATIVE, BRN_PORT_SEISMO_COOPERATIVE, DEFAULT_TTL, 0);
      //BRNPacketAnno::set_ether_anno(p_out, si, brn_etheraddress_broadcast, ETHERTYPE_BRN);

      output(0).push(p_out);

      if ( (no_new_alarm > 0) && (_salm.size() > 2) )
        get_cooperative_alarms();
    }
  }
}

void
SeismoDetectionCooperative::add_alarm(EtherAddress *node, Timestamp *alarmts, uint16_t id)
{
  SeismoAlarmList *node_sal = _salm.find(*node);

  if ( node_sal == NULL ) {
    node_sal = new SeismoAlarmList();
    _salm.insert(*node,node_sal);
  }

  SeismoAlarm *sa = new SeismoAlarm();
  sa->_start = *alarmts;
  sa->_end = *alarmts;
  sa->_id = id;
  node_sal->push_back(sa);
}


/************************************************************************************/
/************************** S I M P L E   M E R G E *********************************/
/************************************************************************************/

bool
SeismoDetectionCooperative::has_alarm(SeismoAlarmList *al, uint32_t t, uint32_t range)
{
  for ( int i = 0; i < al->size(); ++i ) {
    SeismoAlarm *sa = (*al)[i];
    if ((sa->_start.msecval() > (t-range)) && (sa->_start.msecval() < (t+range)))
      return true;
  }

  return false;
}


void
SeismoDetectionCooperative::get_cooperative_alarms()
{
  SeismoAlarmList *group_sal = _salm.find(_group_addr);

  for (NodeAlarmMapIter iter = _salm.begin(); iter.live(); iter++) {

    EtherAddress id = iter.key();

    if ((id ==_group_addr)||(id==_alarm_addr)) continue;

    SeismoAlarmList *sal = iter.value();

    for ( int a = 0; a < sal->size(); ++a ) {

      if ( has_alarm(group_sal,((*sal)[a])->_start.msecval(), _merge_range) ) continue;

      uint32_t min_alarm_time, max_alarm_time, alarm_count;

      min_alarm_time = max_alarm_time = ((*sal)[a])->_start.msecval();
      alarm_count = 1;

      for (NodeAlarmMapIter iter_peer = _salm.begin(); iter_peer.live(); iter_peer++) {
        EtherAddress id_peer = iter_peer.key();

        if ((id_peer==_group_addr)||(id_peer==id)) continue;

        SeismoAlarmList *sal_peer = iter.value();

        for ( int a_peer = 0; a_peer < sal_peer->size(); ++a_peer ) {
          SeismoAlarm *sa_peer = (*sal_peer)[a_peer];
          if ( (sa_peer->_start.msecval() > (max_alarm_time-_merge_range)) &&
               (sa_peer->_start.msecval() < (min_alarm_time+_merge_range)) ) {
            alarm_count++;
            if ( sa_peer->_start.msecval() < min_alarm_time )
              min_alarm_time = sa_peer->_start.msecval();
            else if ( sa_peer->_start.msecval() > max_alarm_time )
              max_alarm_time = sa_peer->_start.msecval();
            break;
          }
        }
      }

      // Two EtherAddress should not be considered (-2): group, alarm
      if ((100 * alarm_count) >= (_fract_threshold * (_salm.size()-2))) {
        //new group alarm
        uint32_t mean_time = (max_alarm_time + min_alarm_time) >> 1;
        SeismoAlarm *sa = new SeismoAlarm();
        sa->_start = Timestamp::make_msec(mean_time);
        sa->_end = Timestamp::make_msec(mean_time);
        sa->_id = _group_alarm_id++;
        group_sal->push_back(sa);
      }
    }
  }

}

/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

enum {
  H_STATS,
  H_ADD_ALARM
};


String
SeismoDetectionCooperative::stats(void)
{
  Timestamp now = Timestamp::now();
  StringAccum sa;

  sa << "<seismodetection_cooperative id=\"" << BRN_NODE_NAME << "\" time=\"";
  sa << now.unparse() << "\" >\n";

  for (NodeAlarmMapIter iter = _salm.begin(); iter.live(); iter++) {

    EtherAddress id = iter.key();
    SeismoAlarmList *sal = iter.value();

    sa << "\t<node id=\"" << id.unparse() << "\" alarmlistsize=\"";
    sa << sal->size() << "\" >\n";
    for ( int i = 0; i < sal->size(); i++) {

      sa << "\t\t<alarm id=\"" << (*sal)[i]->_id << "\" start=\"";
      sa << (*sal)[i]->_start.unparse() << "\" end=\"";
      sa << (*sal)[i]->_end << "\" />\n";
    }

    sa << "\t</node>\n";
  }
  sa << "</seismodetection_cooperative>\n";

  return sa.take_string();
}

static String
read_handler(Element *e, void */*thunk*/)
{
  SeismoDetectionCooperative *sdc = (SeismoDetectionCooperative*)e;
  return sdc->stats();
}

static int
write_handler(const String &in_s, Element *e, void */*thunk*/, ErrorHandler */*errh*/)
{
  SeismoDetectionCooperative *scop = (SeismoDetectionCooperative *)e;

  String s = cp_uncomment(in_s);

  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress node;
  cp_ethernet_address(args[0], &node);

  uint32_t alarmtime;
  cp_integer(args[1], &alarmtime);
  Timestamp alarmts = Timestamp::make_msec(alarmtime,0);

  uint32_t id;
  cp_integer(args[2], &id);

  scop->add_alarm(&node, &alarmts, id);

  return 0;
}
void
SeismoDetectionCooperative::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_handler,H_STATS);
  add_write_handler("add_alarm", write_handler, H_ADD_ALARM);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoDetectionCooperative)
