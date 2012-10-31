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

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "seismo_reporting.hh"
#include "seismodetection_cooperative.hh"

CLICK_DECLS

SeismoDetectionCooperative::SeismoDetectionCooperative():
  _max_alarm(SEISMO_REPORT_MAX_ALARM_COUNT)
{
  BRNElement::init();
}

SeismoDetectionCooperative::~SeismoDetectionCooperative()
{
}

int
SeismoDetectionCooperative::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ALGORITHMS", cpkP+cpkM, cpString, &_algo_string,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

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

  return 0;
}

void
SeismoDetectionCooperative::push(int /*port*/, Packet *p)
{
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
      sa->_start = Timestamp(ntohl(ai[i]._start));
      sa->_end = Timestamp(ntohl(ai[i]._end));
      sa->_id = ntohs(ai[i]._id);
      node_sal->push_back(sa);
      no_new_alarm++;

      if ( node_sal->size() > _max_alarm ) {
        if ( (*node_sal)[0]->_detection_info != NULL )
          //delete (*node_sal)[0]->_detection_info;

        delete (*node_sal)[0];
        node_sal->erase(node_sal->begin());
      }
    }
  }
}

void
SeismoDetectionCooperative::update(SrcInfo */*si*/, uint32_t /*next_new_block*/)
{
  if ( (Timestamp::now() - _last_packet).msecval() > 1000 ) {
    BRN_DEBUG("Trigger");

    SeismoAlarmList *sal = _sdal[0]->get_alarm();
    if ( sal->size() > 0 ) {
      BRN_DEBUG("Alg %d has %d alarms",0,sal->size());
      WritablePacket *new_packet = NULL;

      int psize = sizeof(struct click_seismodetection_coop_header) +
                  sal->size() * sizeof(struct click_seismodetection_alarm_info);

      new_packet = WritablePacket::make( 256, NULL,
       sizeof(struct dht_packet_header) + payload_len, 32);

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


  }

}


/************************************************************************************/
/********************************* H A N D L E R ************************************/
/************************************************************************************/

String
SeismoDetectionCooperative::stats()
{
  Timestamp now = Timestamp::now();
  StringAccum sa;

  sa << "<seismodetection_cooperative time=\"" << now.unparse() << "\" >\n";

  for (NodeAlarmMapIter iter = _salm.begin(); iter.live(); iter++) {

    EtherAddress id = iter.key();
    SeismoAlarmList *sal = iter.value();

    sa << "\t<node id=\"" << id.unparse() << "\" alramlistsize=\"";
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

void
SeismoDetectionCooperative::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", read_handler);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SeismoDetectionCooperative)
