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
 * flowcontrolsource.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "flowcontrolprotocol.hh"
#include "flowcontrolsource.hh"

CLICK_DECLS

FlowControlSource::FlowControlSource():
  _retransmit_timer(static_retransmit_timer_hook,this),
  _etherannos(false),
  _flowid(0)
{
  BRNElement::init();
}

FlowControlSource::~FlowControlSource()
{
}

int
FlowControlSource::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ETHERANNOS", cpkP+cpkM, cpBool, &_etherannos,
      "DEBUG", cpkP, cpUnsigned, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FlowControlSource::initialize(ErrorHandler *)
{
  _retransmit_timer.initialize(this);
  return 0;
}

void
FlowControlSource::static_retransmit_timer_hook(Timer *t, void *f)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((FlowControlSource*)f)->retransmit_data();
  ((FlowControlSource*)f)->set_retransmit_timer();
}

void
FlowControlSource::set_retransmit_timer()
{
  int32_t res = -1;
  int32_t t = -1;
  for (FTIter iter = _flowtab.begin(); iter.live(); iter++) {
    FlowControlInfo *fci = iter.value();

    t = fci->min_age_not_acked();
    if ( (t < res) || (res == -1) ) res = t;
  }

  if ( res != -1 ) {
    _retransmit_timer.reschedule_after_msec(DEFAULT_RETRANSMIT_TIME);
  }
}

void
FlowControlSource::retransmit_data()
{
  for (FTIter iter = _flowtab.begin(); iter.live(); iter++) {
    FlowControlInfo *fci = iter.value();

    Packet *p = fci->get_packet_with_max_age(DEFAULT_RETRANSMIT_TIME);

    while ( p != NULL ) {
      output(0).push(p);
      p = fci->get_packet_with_max_age(DEFAULT_RETRANSMIT_TIME);
    }
  }
}

void
FlowControlSource::push( int port, Packet *packet)
{
  EtherAddress src;
  EtherAddress dst;
  uint16_t ethertype;

  if ( port == 0 ) {
    WritablePacket *etherpacket;
    if ( _etherannos ) {
      src = BRNPacketAnno::src_ether_anno(packet);
      dst = BRNPacketAnno::dst_ether_anno(packet);
      ethertype = BRNPacketAnno::ethertype_anno(packet);
      etherpacket = packet->push(sizeof(uint16_t));
      ((uint16_t*)(etherpacket->data()))[0] = ethertype;
    } else {
      etherpacket = packet->uniqueify();
      click_ether *ether = (click_ether *)etherpacket->data();
      src = EtherAddress(ether->ether_shost);
      dst = EtherAddress(ether->ether_dhost);
      etherpacket->pull(12); //remove both ether addresses
    }

    if ( BRNPacketAnno::flow_ctrl_flags_anno(etherpacket) == FLOW_CTRL_FLOW_START ) {
      BRN_DEBUG("Start Flow");
      if ( ! gen_next_flowid() ) {
        BRN_ERROR("No unused flowid. Drop packet");
        etherpacket->kill();
        return;
      }
      BRN_DEBUG("ft: %d",_flowtab.size());
      FlowControlInfo *new_fci = new FlowControlInfo(_flowid);
      BRN_DEBUG("New FlowID: %d Seq: %d",
                               (uint32_t)(new_fci->_flowid),(uint32_t)(new_fci->_next_seq));
      _flowtab.insert(_flowid,new_fci);
      BRN_DEBUG("ft2: %d p: %p",_flowtab.size(),new_fci);
    }

    BRN_DEBUG("Search for: %d",_flowid);
    FlowControlInfo *fci = _flowtab.find(_flowid);

    uint16_t flowid = fci->_flowid;
    uint16_t seq = fci->_next_seq;

    BRN_DEBUG("FlowID: %d Seq: %d p: %p",(uint32_t)flowid,(uint32_t)seq,fci);

    WritablePacket *p_out = FlowControlProtocol::add_header(etherpacket, FC_TYPE_DATA,
                                                            flowid, seq);

    WritablePacket *out_packet = BRNProtocol::add_brn_header(p_out,BRN_PORT_FLOWCONTROL,
                                                                   BRN_PORT_FLOWCONTROL,
                                                                   255, DEFAULT_TOS);

    BRNPacketAnno::set_ether_anno(out_packet, src, dst, ntohs(ETHERTYPE_BRN));

    int32_t res = fci->insert_packet(out_packet);
    if ( res < 0 ) {
      BRN_ERROR("Error while insert next packet (%d). Drop it",res);
      out_packet->kill();
      //TODO: should never happen. Stop the flow or wait for next free seq
      return;
    }

    if ( ! _retransmit_timer.scheduled() ) {
      _retransmit_timer.schedule_after_msec(DEFAULT_RETRANSMIT_TIME);
    }

    output(0).push(out_packet);

  } else {
    BRN_DEBUG("Receive ack");
    uint16_t type, flowid, seq;

    FlowControlProtocol::get_info(packet, &type, &flowid, &seq);

    EtherAddress src = BRNPacketAnno::src_ether_anno(packet);
    EtherAddress dst = BRNPacketAnno::dst_ether_anno(packet);

    packet->kill();

    FlowControlInfo *fci = _flowtab.find(flowid);

    BRN_DEBUG("Receive ack. Seq : %d",seq);
    fci->clear_packets_up_to_seq(seq);
  }
}

bool
FlowControlSource::gen_next_flowid()
{
  uint16_t s_end = _flowid - 1;
  while ((_flowtab.findp(_flowid) != NULL) && (_flowid != s_end)) _flowid++;
  click_chatter("Flowid = %d", _flowid);
  return (s_end != _flowid);
}

void
FlowControlSource::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowControlSource)
