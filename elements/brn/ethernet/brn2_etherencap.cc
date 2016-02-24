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
 * brnetherencap.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>

#include "brn2_etherencap.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

BRN2EtherEncap::BRN2EtherEncap()
 : _push_header(true),
   _src(EtherAddress()),
   _dst(EtherAddress()),
   _ethertype(0),
   _use_anno(false),
   _set_src(false),
   _set_dst(false),
   _set_fixed_values(false)
{
  BRNElement::init();
}

BRN2EtherEncap::~BRN2EtherEncap()
{
}

int
BRN2EtherEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "SRC", cpkP, cpEthernetAddress, &_src,
      "DST", cpkP, cpEthernetAddress, &_dst,
      "ETHERTYPE", cpkP, cpUnsignedShort, &_ethertype,
      "USEANNO", cpkP, cpBool, &_use_anno,
      "PUSHHEADER", cpkP, cpBool, &_push_header,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _set_src = _src.is_set();
  _set_dst = _dst.is_set();

  _set_fixed_values = (_set_dst) || (_set_src) || (_ethertype!=0);

  return 0;
}

Packet *
BRN2EtherEncap::smaction(Packet *p)
{
  WritablePacket *q = p->uniqueify();

  if (!q) {
    p->kill();
    return NULL;
  }

  if (_push_header) q = q->push(14);

  click_ether *ether = reinterpret_cast<click_ether *>(q->data());
  click_ether *annotated_ether = reinterpret_cast<click_ether *>(q->ether_header());

  if (_set_fixed_values) {

    BRN_DEBUG("Setting Etherheader: %s (%d) -> %s (%d) %d (%d).", _src.unparse().c_str(), (_set_src?1:0),
                                                                  _dst.unparse().c_str(), (_set_dst?1:0),
                                                                  ntohs(_ethertype),(_ethertype!=0)?1:0);

    if (_set_src) memcpy(ether->ether_shost, _src.data(), 6);
    if (_set_dst) memcpy(ether->ether_dhost, _dst.data(), 6);
    if (_ethertype!=0) ether->ether_type = htons(_ethertype);

  } else if (_use_anno) {  // are the mac annotations available?

    BRN_INFO("Set Header: %s %s %d",(BRNPacketAnno::src_ether_anno(q)).unparse().c_str(),
                                    (BRNPacketAnno::dst_ether_anno(q)).unparse().c_str(),
                                    htons(BRNPacketAnno::ethertype_anno(q)));

    memcpy(ether->ether_shost, (BRNPacketAnno::src_ether_anno(q)).data(), 6);
    memcpy(ether->ether_dhost, (BRNPacketAnno::dst_ether_anno(q)).data(), 6);
    ether->ether_type = htons(BRNPacketAnno::ethertype_anno(q));

  } else {

    if ( annotated_ether == NULL ) {
      BRN_ERROR("annotated_ether isn't set!");
    } else {
      if ( (void*)annotated_ether != (void*)ether ) {
        memcpy(ether->ether_shost, annotated_ether->ether_shost, 6);
        memcpy(ether->ether_dhost, annotated_ether->ether_dhost, 6);
        ether->ether_type = annotated_ether->ether_type;
      } else {
        BRN_INFO("annotated_ether == ether! no copy required.");
      }
    }
  }

  q->set_ether_header(ether);

  return q;
}

void
BRN2EtherEncap::push(int, Packet *p)
{
  if (Packet *q = smaction(p)) {
    output(0).push(q);
  }
}

Packet *
BRN2EtherEncap::pull(int)
{
  if (Packet *p = input(0).pull()) {
    return smaction(p);
  } else {
    return 0;
  }
}

Packet *
BRN2EtherEncap::push_ether_header(Packet *p, uint8_t *src, uint8_t *dst, uint16_t ethertype)
{
  WritablePacket *q = p->push(14);
  if (q) {
    click_ether *ether = reinterpret_cast<click_ether *>(q->data());
    memcpy(ether->ether_shost, src, 6);
    memcpy(ether->ether_dhost, dst, 6);
    ether->ether_type = htons(ethertype);
  } else {
    click_chatter("No space for etherheader");
    return NULL;
  }

  return q;
}

Packet *
BRN2EtherEncap::push_ether_header(Packet *p, EtherAddress *src, EtherAddress *dst, uint16_t ethertype)
{
  WritablePacket *q = p->push(14);
  if (q) {
    click_ether *ether = reinterpret_cast<click_ether *>(q->data());
    memcpy(ether->ether_shost, src->data(), 6);
    memcpy(ether->ether_dhost, dst->data(), 6);
    ether->ether_type = htons(ethertype);
  } else {
    click_chatter("No space for etherheader");
    return NULL;
  }

  return q;
}

void
BRN2EtherEncap::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BRNElement)
EXPORT_ELEMENT(BRN2EtherEncap)
