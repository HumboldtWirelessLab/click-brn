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

#include "dhtprotocol_dart.hh"
#include "dart_routingtable.hh"
#include "dart_linkprobe_handler.hh"

CLICK_DECLS

DartLinkProbeHandler::DartLinkProbeHandler():
    _drt(NULL),
    _linkstat(NULL),
    _debug(BrnLogger::DEFAULT)
{
}

DartLinkProbeHandler::~DartLinkProbeHandler()
{
}

int
DartLinkProbeHandler::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DRT", cpkP+cpkM, cpElement, &_drt,
      "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

/**
 * src is the Etheraddress of the source of the LP if lp is received (see direction)
 */

static int
tx_handler(void *element, const EtherAddress */*src*/, char *buffer, int size)
{
  DartLinkProbeHandler *dhtd = (DartLinkProbeHandler*)element;
  return dhtd->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress */*src*/, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  DartLinkProbeHandler *dhtd = (DartLinkProbeHandler*)element;
  return dhtd->lpReceiveHandler(buffer, size);
}
int
DartLinkProbeHandler::initialize(ErrorHandler *)
{
  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_DHT_DART, &tx_handler, &rx_handler);
  return 0;
}

int
DartLinkProbeHandler::lpSendHandler(char *buffer, int size)
{
  int len;

  if ( ! _drt->_validID ) return -1;  //return -1 to signal that we have no information

  BRN_DEBUG("Pack Linkprobe data.");

  len = DHTProtocolDart::pack_lp((uint8_t*)buffer, size, _drt->_me, NULL);
  return len;
}

int
DartLinkProbeHandler::lpReceiveHandler(char *buffer, int size)
{
//  int len;
  DHTnode first;
  DHTnodelist nodes;

  BRN_DEBUG("Unpack Linkprobe data. Size: %d",size);
  len = DHTProtocolDart::unpack_lp((uint8_t*)buffer, size, &first, &nodes);

  _drt->add_neighbour(&first);
  _drt->add_nodes(&nodes);
  /* Just add. No other element need to be informed. So don't cal drt->update here.*/

  nodes.del();

  return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolDart)
EXPORT_ELEMENT(DartLinkProbeHandler)
