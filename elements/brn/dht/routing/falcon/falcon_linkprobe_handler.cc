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
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/dht/standard/dhtnode.hh"
#include "elements/brn/dht/standard/dhtnodelist.hh"
#include "elements/brn/dht/protocol/dhtprotocol.hh"

#include "dhtprotocol_falcon.hh"
#include "falcon_routingtable.hh"
#include "falcon_linkprobe_handler.hh"

CLICK_DECLS

FalconLinkProbeHandler::FalconLinkProbeHandler():
    _register_handler(true),
    _frt(NULL),
    _linkstat(NULL),
    _all_nodes_index(0),
    _no_nodes_per_lp(FALCON_DEFAULT_NO_NODES_PER_LINKPROBE),
    _rfrt(NULL),
    _active(false),
    _delay(0),
    _onlyfingertab(false)
{
  BRNElement::init();
}

FalconLinkProbeHandler::~FalconLinkProbeHandler()
{
}

int
FalconLinkProbeHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "FRT", cpkP+cpkM, cpElement, &_frt,
      "LINKSTAT", cpkP+cpkM, cpElement, &_linkstat,
      "REGISTERHANDLER", cpkP, cpBool, &_register_handler,
      "ONLYFINGERTAB", cpkP, cpBool, &_onlyfingertab,
      "NODESPERLP", cpkN, cpInteger, &_no_nodes_per_lp,
      "DELAY", cpkN, cpInteger, &_delay,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;


  _active = (_delay == 0);

  //click_chatter("ACTIVE: %s Delay: %d", String(_active).c_str(),_delay);
  _start = Timestamp::now();

  return 0;
}

/**
 * src is the Etheraddress of the source of the LP if lp is received (see direction)
 */

static int32_t
tx_handler(void *element, const EtherAddress */*src*/, char *buffer, int32_t size)
{
  FalconLinkProbeHandler *dhtf = (FalconLinkProbeHandler*)element;
  if ( dhtf == NULL ) return 0;

  return dhtf->lpSendHandler(buffer, size);
}

static int32_t
rx_handler(void *element, EtherAddress */*src*/, char *buffer, int32_t size, bool is_neighbour, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  FalconLinkProbeHandler *dhtf = (FalconLinkProbeHandler*)element;
  if ( dhtf == NULL ) return 0;

  return dhtf->lpReceiveHandler(buffer, size, is_neighbour);
}

int
FalconLinkProbeHandler::register_linkprobehandler()
{
  if ( _register_handler )
    _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_DHT_FALCON, &tx_handler, &rx_handler);

  return 0;
}

int
FalconLinkProbeHandler::initialize(ErrorHandler *)
{
  register_linkprobehandler();

  return 0;
}

int32_t
FalconLinkProbeHandler::lpSendHandler(char *buffer, int32_t size)
{
  int32_t len = 0, send_nodes = 0;

  if ( ! _active ) {
    if ( (Timestamp::now() - _start).msecval() >= _delay ) _active = true;
    else return len;
  }

  BRN_DEBUG("Send");

  DHTnodelist nodes;

  if ( _onlyfingertab ) {
    send_nodes = MIN(MIN(_no_nodes_per_lp, _frt->_fingertable.size()),DHTProtocolFalcon::max_no_nodes_in_lp(size));

    for (int32_t i = 0; i < send_nodes; i++ ) {
      _all_nodes_index = ( _all_nodes_index + 1 ) % _frt->_fingertable.size();
      nodes.add_dhtnode(_frt->_fingertable.get_dhtnode(_all_nodes_index));
    }
  } else {
    send_nodes = MIN(MIN(_no_nodes_per_lp, _frt->_allnodes.size()),DHTProtocolFalcon::max_no_nodes_in_lp(size));

    for (int32_t i = 0; i < send_nodes; i++ ) {
      _all_nodes_index = ( _all_nodes_index + 1 ) % _frt->_allnodes.size();
      nodes.add_dhtnode(_frt->_allnodes.get_dhtnode(_all_nodes_index));
    }
  }

  len = DHTProtocolFalcon::pack_lp((uint8_t*)buffer, size, _frt->_me, &nodes);

  BRN_DEBUG("Send nodes: %d Size: %d Len: %d nodes_per_lp: %d Max_nodes_per_lp: %d",
                                                                     send_nodes,size,len,_no_nodes_per_lp,
                                                                     DHTProtocolFalcon::max_no_nodes_in_lp(size));

  nodes.clear();

  return len;
}

int32_t
FalconLinkProbeHandler::lpReceiveHandler(char *buffer, int32_t size, bool is_neighbour)
{
  int32_t len;
  DHTnode first;
  DHTnodelist nodes;
  DHTnode *add_node;

  if ( ! _active ) {
    BRN_DEBUG("Not active. Time since start: %d. delay: %d", (Timestamp::now() - _start).msecval(),_delay);
    if ( (Timestamp::now() - _start).msecval() >= _delay ) _active = true;
    else return 0;
  }

  BRN_DEBUG("Receive. Neighbour: %s", String(is_neighbour).c_str());

  len = DHTProtocolFalcon::unpack_lp((uint8_t*)buffer, size, &first, &nodes);
  if ( len == -1 ) {
    BRN_WARN("Error on linkprobe unpack");
  }


  BRN_DEBUG("Address: %s",first._ether_addr.unparse().c_str());
  BRN_DEBUG("Linkprobe: %d node in the linkprobe.", nodes.size() + 1 );

  if ( is_neighbour) {
    _frt->add_neighbour(&first);
  } else {
    _frt->add_node(&first);
  }

  if ( (_rfrt == NULL) || (is_neighbour) ) {  //disable if hawk is used and first is not a neighbour to avoid unreachable successor
    _frt->add_nodes(&nodes);
  }

  //Add Neighbour (src of lp)
  if ( (_rfrt != NULL) && (is_neighbour) ) {
    //add first as next phy hop for set "nodes"
    _rfrt->addEntry(&(first._ether_addr), first._md5_digest, first._digest_length, &(first._ether_addr));

    /* TODO: Knoten nur hinzufügen, wenn sie noch gar nicht bekannt sind
    for (int32_t i = 0; i < nodes.size(); i++ ) {
      add_node = nodes.get_dhtnode(i);
      _rfrt->addEntry(&(add_node->_ether_addr), add_node->_md5_digest, add_node->_digest_length, &(first._ether_addr));
    }*/
  }

  nodes.del();

  return 0;
}

static int
write_reg_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  FalconLinkProbeHandler *lph = (FalconLinkProbeHandler *)e;

  String s = cp_uncomment(in_s);
  bool reg_handler;
  if (!cp_bool(s, &reg_handler))
    return errh->error("register_handler parameter must be a bool");

  if ( reg_handler != lph->_register_handler ) {
    lph->_register_handler = reg_handler;
    lph->register_linkprobehandler();
  }

  return 0;
}

void
FalconLinkProbeHandler::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("register_handler", write_reg_param , (void *)0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocol)
EXPORT_ELEMENT(FalconLinkProbeHandler)
