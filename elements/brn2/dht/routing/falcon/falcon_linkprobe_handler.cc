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

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn2/dht/standard/dhtnode.hh"
#include "elements/brn2/dht/standard/dhtnodelist.hh"
#include "elements/brn2/dht/protocol/dhtprotocol.hh"

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
    _rfrt(NULL)
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
      "NODESPERLP", cpkN, cpInteger, &_no_nodes_per_lp,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

/**
 * src is the Etheraddress of the source of the LP if lp is received (see direction)
 */

static int
handler(void *element, EtherAddress */*src*/, char *buffer, int size, bool direction)
{
  FalconLinkProbeHandler *dhtf = (FalconLinkProbeHandler*)element;

  if ( direction )
    return dhtf->lpSendHandler(buffer, size);
  else
    return dhtf->lpReceiveHandler(buffer, size);

  if ( dhtf == NULL ) return 0;

  return 0;
}

int
FalconLinkProbeHandler::register_linkprobehandler()
{
  if ( _register_handler ) _linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_DHT_FALCON,&handler);
  return 0;
}

int
FalconLinkProbeHandler::initialize(ErrorHandler *)
{
  register_linkprobehandler();

  return 0;
}

int
FalconLinkProbeHandler::lpSendHandler(char *buffer, int size)
{
  int len, send_nodes;

  BRN_DEBUG("Send");

  DHTnodelist nodes;

  send_nodes = min(_no_nodes_per_lp, _frt->_allnodes.size());

  for (int i = 0; i < send_nodes; i++ ) {
    _all_nodes_index = ( _all_nodes_index + 1 ) % _frt->_allnodes.size();
    nodes.add_dhtnode(_frt->_allnodes.get_dhtnode(_all_nodes_index));
  }

  len = DHTProtocolFalcon::pack_lp((uint8_t*)buffer, size, _frt->_me, &nodes);

  nodes.clear();

  return len;
}

int
FalconLinkProbeHandler::lpReceiveHandler(char *buffer, int size)
{
  int len;
  DHTnode first;
  DHTnodelist nodes;

  BRN_DEBUG("receive");

  len = DHTProtocolFalcon::unpack_lp((uint8_t*)buffer, size, &first, &nodes);

  BRN_DEBUG("Address: %s",first._ether_addr.unparse().c_str());
  BRN_DEBUG("Linkprobe: %d node in the linkprobe.", nodes.size() + 1 );
  _frt->add_neighbour(&first);
  _frt->add_nodes(&nodes);

  nodes.del();

  if ( _rfrt != NULL )
    _rfrt->addEntry(&(first._ether_addr), first._md5_digest, first._digest_length, &(first._ether_addr));

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
