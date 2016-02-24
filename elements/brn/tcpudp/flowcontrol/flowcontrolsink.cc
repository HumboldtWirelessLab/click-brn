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
 * FlowControlSink.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "flowcontrolprotocol.hh"
#include "flowcontrolsink.hh"

CLICK_DECLS

FlowControlSink::FlowControlSink()
{
  BRNElement::init();
}

FlowControlSink::~FlowControlSink()
{
  clear_flowtab();
}

int
FlowControlSink::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpUnsigned, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
FlowControlSink::initialize(ErrorHandler *)
{
  return 0;
}

void
FlowControlSink::push( int /*port*/, Packet *packet )
{
  uint16_t type, flowid, seq;

  FlowControlProtocol::get_info(packet, &type, &flowid, &seq);

  BRN_DEBUG("Flow: %d %d",flowid,seq);

  if ( _flowtab.findp(flowid) == NULL ) {
   FlowControlInfo *new_fci = new FlowControlInfo(flowid);
   _flowtab.insert(flowid,new_fci);
  }

  FlowControlInfo *fci = _flowtab.find(flowid);

  FlowControlProtocol::strip_header(packet);

  EtherAddress src = BRNPacketAnno::src_ether_anno(packet);
  EtherAddress dst = BRNPacketAnno::dst_ether_anno(packet);
  uint16_t ethertype = ntohs(((uint16_t*)(packet->data()))[0]);
  packet->pull(sizeof(uint16_t));

  BRNPacketAnno::set_ether_anno(packet, src, dst, ntohs(ethertype));

  if ((fci->has_packet(seq)) || (!fci->packet_fits_in_window(seq))) {
    BRN_WARN("Dup. packet or it doesn't fit");
    packet->kill();

    if ( fci->_min_seq == 0 ) return;
    WritablePacket *ack_p = FlowControlProtocol::make_ack(fci->_flowid,
                                                         fci->_min_seq-1);

    WritablePacket *out_ack_p = BRNProtocol::add_brn_header(ack_p,
                                                  BRN_PORT_FLOWCONTROL,
                                                  BRN_PORT_FLOWCONTROL,
                                                  255, DEFAULT_TOS);

    //switch src & dst: its a reply
    BRNPacketAnno::set_ether_anno(out_ack_p, dst, src, ETHERTYPE_BRN);

    output(1).push(out_ack_p);
    return;
  }

  fci->insert_packet(packet,seq);

  Packet *p = fci->get_and_clear_acked_seq();

  if ( p == NULL ) {
    BRN_DEBUG("No packets");
    return;
  }

  while ( p != NULL ) {
    output(0).push(p);
    p = fci->get_and_clear_acked_seq();
  }

  BRN_DEBUG("%s",fci->print_info().c_str());

  if ( fci->_min_seq == 0 ) return;

  WritablePacket *ack_p = FlowControlProtocol::make_ack(fci->_flowid, fci->_min_seq-1);

  WritablePacket *out_ack_p = BRNProtocol::add_brn_header(ack_p, BRN_PORT_FLOWCONTROL,
                                                                 BRN_PORT_FLOWCONTROL,
                                                                 255, DEFAULT_TOS);

  //switch src & dst: its a reply
  BRNPacketAnno::set_ether_anno(out_ack_p, dst, src, ETHERTYPE_BRN);

  output(1).push(out_ack_p);
}

void
FlowControlSink::clear_flowtab()
{
  for (FTIter iter = _flowtab.begin(); iter.live(); ++iter) {
    FlowControlInfo *fci = iter.value();

    delete fci;
  }
}

void
FlowControlSink::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FlowControlSink)
