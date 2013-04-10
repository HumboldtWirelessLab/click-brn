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
 * srcforwarder.{cc,hh} -- forwards dsr source routed packets
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/dht/routing/dart/dart_functions.hh"

#include "dart_protocol.hh"
#include "dart_forwarder.hh"


CLICK_DECLS

DartForwarder::DartForwarder()
  : _debug(BrnLogger::DEFAULT),
    _opt(0),
    _me()
    
{

}

DartForwarder::~DartForwarder()
{
}

int
DartForwarder::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "DARTIDCACHE", cpkP+cpkM, cpElement, &_idcache,
      "DARTROUTING", cpkP+cpkM, cpElement, &_dartrouting,
      "DRT", cpkP+cpkM, cpElement, &_drt,
      "DEBUG", cpkN, cpInteger, &_debug,
      "OPT", cpkN, cpInteger, &_opt,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("BRN2NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
DartForwarder::initialize(ErrorHandler *)
{
  return 0;
}

void
DartForwarder::uninitialize()
{
  //cleanup
}

void
DartForwarder::push(int port, Packet *p_in)
{
  BRN_DEBUG("push()");

  struct dart_routing_header *header = DartProtocol::get_route_header(p_in);
  click_ether *ether = DartProtocol::get_ether_header(p_in);

  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  uint8_t ttl = BRNPacketAnno::ttl_anno(p_in);
  if ( port == 0 ) ttl--;
  BRN_DEBUG("Src: %s (%s) Dst: %s (%s) TTL: %d Port: %d", src_addr.unparse().c_str(),
                                 DartFunctions::print_id(header->_src_nodeid, ntohl(header->_src_nodeid_length)).c_str(),
                                                          dst_addr.unparse().c_str(),
                                 DartFunctions::print_id(header->_dst_nodeid, ntohl(header->_dst_nodeid_length)).c_str(),
                                                          (int)ttl,port);

  if ( _idcache->getEntry(&dst_addr) == NULL ) _idcache->addEntry(&dst_addr, header->_dst_nodeid, ntohl(header->_dst_nodeid_length));
  if ( _idcache->getEntry(&src_addr) == NULL ) _idcache->addEntry(&src_addr, header->_src_nodeid, ntohl(header->_src_nodeid_length));


  if ( memcmp(dst_addr.data(),_dartrouting->_me->_ether_addr.data(),6) == 0 ) {
    BRN_DEBUG("Is for me");

    DartProtocol::strip_route_header(p_in);

    if ( BRNProtocol::is_brn_etherframe(p_in) )
      BRNProtocol::get_brnheader_in_etherframe(p_in)->ttl = ttl;

    output(1).push(p_in);
  } else {

    DHTnode *n = _drt->get_neighbour(&dst_addr);  //check whether destination is a neighbouring node

    if ( n == NULL ){
     if (_opt == 1)
      n = _dartrouting->get_responsibly_node_opt(header->_dst_nodeid);
     else
      n = _dartrouting->get_responsibly_node(header->_dst_nodeid);
}
    BRN_DEBUG("Responsible is %s",n->_ether_addr.unparse().c_str());

    if ( n->equalsEtherAddress(_dartrouting->_me) ) {         //for clients, which have the same id but different ethereaddress
      DartProtocol::strip_route_header(p_in);                 //TODO: use other output port for client

      if ( BRNProtocol::is_brn_etherframe(p_in) )
        BRNProtocol::get_brnheader_in_etherframe(p_in)->ttl = ttl;

      output(1).push(p_in);
    } else {

      if ( ttl == 0 ) {
        BRN_WARN("TTL is 0. Kill the packet.");
        p_in->kill();
      } else {

        Packet *brn_p = BRNProtocol::add_brn_header(p_in, BRN_PORT_DART, BRN_PORT_DART, ttl, BRNPacketAnno::tos_anno(p_in));

        BRNPacketAnno::set_src_ether_anno(brn_p,_dartrouting->_me->_ether_addr);  //TODO: take address from anywhere else
        BRNPacketAnno::set_dst_ether_anno(brn_p,n->_ether_addr);
        BRNPacketAnno::set_ethertype_anno(brn_p,ETHERTYPE_BRN);

        output(0).push(brn_p);
      }
    }
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  DartForwarder *sf = (DartForwarder *)e;
  return String(sf->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  DartForwarder *sf = (DartForwarder *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  sf->_debug = debug;
  return 0;
}

void
DartForwarder::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(DartForwarder)
