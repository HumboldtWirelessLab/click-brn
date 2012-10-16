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
 * BatmanFailureDetection.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "batmanfailuredetection.hh"
#include "batmanroutingtable.hh"
#include "batmanprotocol.hh"
#include "elements/brn/brn2.h"

CLICK_DECLS

BatmanFailureDetection::BatmanFailureDetection():
  _active(false),
  _loop_timeout(BATMAN_DEFAULT_LOOP_TIMEOUT),
  _detected_loops(0), _detected_duplicates(0), _no_retries(0), _no_failures(0)
{
  BRNElement::init();
}

BatmanFailureDetection::~BatmanFailureDetection()
{
}

int
BatmanFailureDetection::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEID", cpkP+cpkM, cpElement, &_nodeid,
      "BATMANTABLE", cpkP+cpkM, cpElement, &_brt,
      "ACTIVE", cpkP, cpBool, &_active,
      "LOOPTIMEOUT", cpkP, cpInteger, &_loop_timeout,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
BatmanFailureDetection::initialize(ErrorHandler *)
{
  return 0;
}

void
BatmanFailureDetection::push( int port, Packet *packet )
{
  if ( _active ) {

    if ( port == BATMAN_FAILURE_DETECTION_INPORT_TXFEEDBACK ) packet = BRNProtocol::pull_brn_header(packet);

    Timestamp now = Timestamp::now(); //TODO: use packet timestamp
    batman_routing *br = BatmanProtocol::get_batman_routing(packet);
    uint16_t packet_id = ntohs(br->id);
    click_ether *et = BatmanProtocol::get_ether_header(packet);
    EtherAddress dst = EtherAddress(et->ether_dhost);
    EtherAddress src = EtherAddress(et->ether_shost);

    if ( port == BATMAN_FAILURE_DETECTION_INPORT_RX ) {
      uint8_t ttl = BRNPacketAnno::ttl_anno(packet) - 1;

      BatmanSrcInfo *bsi = _bsit.findp(src);

      if ( bsi == NULL ) {
        _bsit.insert(src, BatmanSrcInfo());
        bsi = _bsit.findp(src);
      }

      int error;
      if ( (error = bsi->packet_error_type(packet_id, &now, _loop_timeout, ttl, true)) != BATMAN_ERROR_NONE ) {
        if ( error == BATMAN_ERROR_LOOP ) {
          BRN_DEBUG("Loop detected: Src: %s Dst: %s TTL: %d ID: %d", src.unparse().c_str(),
                                                                    dst.unparse().c_str(),
                                                                    (int)ttl, (int)packet_id);
          _detected_loops++;
          output(BATMAN_FAILURE_DETECTION_OUTPORT_ERROR).push(packet);
        } else {
          BRN_DEBUG("Duplicate detected: Src: %s Dst: %s TTL: %d ID: %d", src.unparse().c_str(),
                                                                    dst.unparse().c_str(),
                                                                    (int)ttl, (int)packet_id);
          _detected_duplicates++;
          packet->kill();
        }
      } else {
        //BRN_WARN("Forward: Src: %s Dst: %s TTL: %d ID: %d",src.unparse().c_str(), dst.unparse().c_str(),
        //                                                   (int)ttl, (int)packet_id);
        output(BATMAN_FAILURE_DETECTION_OUTPORT_RX).push(packet);
      }

      return;

    } else { //TXFEEDBACK
      BatmanSrcInfo *bsi = _bsit.findp(src);
      if ( bsi == NULL ) {
        _bsit.insert(src, BatmanSrcInfo());
        bsi = _bsit.findp(src);
      }

      int retries = bsi->packet_retry(packet_id, &now, _loop_timeout); //TODO: use othe timeout

      if ( retries < 1 ) {
        bsi->inc_retry(packet_id);
        //BRN_WARN("Retry: Src: %s Dst: %s Retries: %d ID: %d", src.unparse().c_str(), dst.unparse().c_str(),
        //                                                      (int)retries, (int)packet_id);
        _no_retries++;
        output(BATMAN_FAILURE_DETECTION_OUTPORT_TX_RETRY).push(BRNProtocol::push_brn_header(packet));
      } else {
        BRN_DEBUG("Too much retries: Src: %s Dst: %s Retries: %d ID: %d", src.unparse().c_str(),
                                                                         dst.unparse().c_str(),
                                                                         (int)retries, (int)packet_id);
        _no_failures++;
        packet->kill();
      }

      return;
    }
  } else {
    if ( port == BATMAN_FAILURE_DETECTION_INPORT_RX )
      output(BATMAN_FAILURE_DETECTION_OUTPORT_RX).push(packet);
    else
      packet->kill();
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

String
BatmanFailureDetection::get_info()
{
  StringAccum sa;

  sa << "<batmanfailuredetection node=\"" << _nodeid->getMasterAddress()->unparse().c_str();
  sa << "\" loops=\"" << _detected_loops << "\" duplicates=\"" << _detected_duplicates << "\" retries=\"";
  sa << _no_retries << "\" failures=\"" << _no_failures << "\" >\n</batmanfailuredetection>\n";

  return sa.take_string();

}

static String 
BatmanFailureDetection_read_param(Element *e, void */*thunk*/)
{
  return ((BatmanFailureDetection*)e)->get_info();
}

void
BatmanFailureDetection::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("info", BatmanFailureDetection_read_param, (void *)0);

}

CLICK_ENDDECLS
ELEMENT_REQUIRES(BatmanProtocol)
EXPORT_ELEMENT(BatmanFailureDetection)
