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
#include "dart_functions.hh"

#include "dhtprotocol_dart.hh"
#include "dart_routingtable.hh"
#include "dart_linkprobe_handler.hh"

CLICK_DECLS

DartLinkProbeHandler::DartLinkProbeHandler():
    _drt(NULL),
    _linkstat(NULL),
    _neighbour_nodes_index(0),
    _no_nodes_per_lp(DART_DEFAULT_NO_NODES_PER_LINKPROBE),
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
tx_handler(void *element, const EtherAddress */*src*/, char *buffer, int32_t size)
{
  DartLinkProbeHandler *dhtd = reinterpret_cast<DartLinkProbeHandler*>(element);
  return dhtd->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress */*src*/, char *buffer, int32_t size, bool is_neighbour, uint8_t fwd_rate, uint8_t rev_rate)
{
  DartLinkProbeHandler *dhtd = reinterpret_cast<DartLinkProbeHandler*>(element);
  if ( (fwd_rate < 10) || (rev_rate < 10) ) is_neighbour = false;
  return dhtd->lpReceiveHandler(buffer, size, is_neighbour);
}
int
DartLinkProbeHandler::initialize(ErrorHandler *)
{
  _linkstat->registerHandler(this, BRN2_LINKSTAT_MINOR_TYPE_DHT_DART, &tx_handler, &rx_handler);
  return 0;
}

int
DartLinkProbeHandler::lpSendHandler(char *buffer, int32_t size)
{
  int len;
  DHTnodelist nodes;
  int send_nodes;

  if ( ! _drt->_validID ) return -1;  //return -1 to signal that we have no information

  BRN_DEBUG("Pack Linkprobe data.");

  send_nodes = MIN(MIN(_no_nodes_per_lp, _drt->_neighbours.size()),
  /*DHTProtocolDart::max_no_nodes_in_lp(size)*/ 5);
  for (int32_t i = 0; i < send_nodes; i++ ) {
    _neighbour_nodes_index = ( _neighbour_nodes_index + 1 ) % _drt->_neighbours.size();
     DHTnode *next = _drt->_neighbours[_neighbour_nodes_index]->neighbour;//.get_dhtnode(_neighbour_nodes_index);
    if (memcmp(_drt->_me->_ether_addr.data(),next->_ether_addr.data(),6) != 0)
    nodes.add_dhtnode(next);
  }

  len = DHTProtocolDart::pack_lp((uint8_t*)buffer, size, _drt->_me, &nodes, _drt->_ident);
  return len;
}

int
DartLinkProbeHandler::lpReceiveHandler(char *buffer, int32_t size,bool is_neighbour)
{
//  int len;
  DHTnode first;
  DHTnodelist nodes;
  EtherAddress ident = EtherAddress();
  BRN_DEBUG("Unpack Linkprobe data. Size: %d",size);
  /*len =*/ DHTProtocolDart::unpack_lp((uint8_t*)buffer, size, &first, &nodes,&ident);

  if ( _drt->_ds->_lt->get_host_metric_to_me(first._ether_addr) < 300 ) is_neighbour = true;
  else is_neighbour = false;

 /*  if (is_neighbour && memcmp(_drt->_ident,ident.data(),6) != 0){
   BRN_DEBUG("Receive ident : %s",ident.unparse().c_str());
	EtherAddress my_ident = EtherAddress();
       memcpy(my_ident.data(),_drt->_ident,6); 
 if (my_ident.sdata()[0] > ident.sdata()[0] || 
	( my_ident.sdata()[0] == ident.sdata()[0] && my_ident.sdata()[1] > ident.sdata()[1]) ||
	( my_ident.sdata()[0] == ident.sdata()[0] && my_ident.sdata()[1] == ident.sdata()[1] && my_ident.sdata()[2] > ident.sdata()[2])
	){
       BRN_DEBUG("clear my tables ");
	_drt->_validID = false;
      memcpy(_drt->_ident,ident.data(),6);
      _drt->_neighbours.clear();
      _drt->_allnodes.clear();
      _drt->update_callback(DART_CLEAR_STORAGE);
 }
}*/

  if (is_neighbour /*&& memcmp(_drt->_ident,ident.data(),6) == 0*/) {

    BRN_DEBUG("is neighbour: %s",String(is_neighbour).c_str());
    _drt->add_neighbour(&first);

    //dont update my own ID cause this is already done with table_maintenance
    for(int i=0;i<nodes.size();i++){
      DHTnode *node = nodes.get_dhtnode(i);
      if(memcmp(_drt->_me->_ether_addr.data(),node->_ether_addr.data(),6) == 0){
        nodes.erase_dhtnode(&(node->_ether_addr));break;
      }
    }
    _drt->add_nodes(&nodes);
    //important to do this after add_nodes cause cloning nodes is in add_nodes
    _drt->add_neighbours_neighbour(&first,&nodes);
  }
  /* Just add. No other element need to be informed. So don't cal drt->update here.*/

  nodes.del();

  return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(DHTProtocolDart)
EXPORT_ELEMENT(DartLinkProbeHandler)
