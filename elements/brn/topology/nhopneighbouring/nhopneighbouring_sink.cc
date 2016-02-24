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
 * eventnotifier.{cc,hh}
 */

#include <click/config.h>

#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnprotocol.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "nhopneighbouring_protocol.hh"
#include "nhopneighbouring_sink.hh"

CLICK_DECLS

NHopNeighbouringSink::NHopNeighbouringSink()
:nhop_info(NULL),_link_table(NULL){
  BRNElement::init();
}

NHopNeighbouringSink::~NHopNeighbouringSink()
{
}

int
NHopNeighbouringSink::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NHOPN_INFO", cpkP, cpElement, &nhop_info,
      "LINKTABLE", cpkP, cpElement, &_link_table,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
NHopNeighbouringSink::initialize(ErrorHandler *)
{
  return 0;
}

/**
 * 
 * @param  
 * @param packet 
 */
void
NHopNeighbouringSink::push( int /*port*/, Packet *packet )
{
  uint8_t hops;
  uint8_t hop_limit;
  uint16_t no_neighbours;
  EtherAddress src;

  BRN_DEBUG("Received Paket");
  NHopNeighbouringProtocol::unpack_ping(packet, &src, &no_neighbours, &hop_limit, &hops);

  //TODO: use Linkstat to check whether this is a valid neighbour,
  //Only if so update his values
  if ( (! _link_table) || (hops > 1) ) {
    nhop_info->update_neighbour(&src, hops, hop_limit, no_neighbours);
  } else {
    if (_link_table->get_host_metric_from_me(src) < 200) {
      click_chatter("Link-metric is %d",_link_table->get_host_metric_from_me(src));
      nhop_info->update_neighbour(&src, hops, hop_limit, no_neighbours);
      //NHopNeighbouringCompressedProtocol::unpack_nhop(packet, &src, &hop_limit, nhop_info); //compressed
    }
  }
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
NHopNeighbouringSink::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NHopNeighbouringSink)
