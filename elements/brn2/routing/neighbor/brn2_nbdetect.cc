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
 * nblist.{cc,hh} -- a list of information about neighbor nodes (brn-nodes)
 * A. Zubow
 */

#include <click/config.h>
#include "brn2_nbdetect.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

NeighborDetect::NeighborDetect()
// : _debug(BrnLogger::DEFAULT)
{
//  _nb_list = new NBMap();
}

NeighborDetect::~NeighborDetect()
{
//  BRN_DEBUG(" * current nb list: %s", printNeighbors().c_str());

//  delete _nb_list;
}

int
    NeighborDetect::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NBLIST", cpkP+cpkM, cpElement, &_nblist,
      "DEVICE", cpkP+cpkM, cpElement, &_device,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
NeighborDetect::initialize(ErrorHandler *)
{
  return 0;
}

/* should be an annotated brn packet */
Packet *
NeighborDetect::simple_action(Packet *p_in)
{
  //const click_ether *ether = (const click_ether *)p_in->data();
  const click_ether *ether = (click_ether *)p_in->ether_header();

  if (ether) {
    _nblist->insert(EtherAddress(ether->ether_shost), _device);
  }

  return p_in;
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element */*e*/, void *)
{
  //NeighborDetect *nl = (NeighborDetect *)e;
  return /*String(nl->_debug) + */"not supported\n";
}

static int 
write_debug_param(const String &in_s, Element */*e*/, void *,
		      ErrorHandler *errh)
{
  //NeighborDetect *nl = (NeighborDetect *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  //nl->_debug = debug;
  return 0;
}

void
NeighborDetect::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);

//  add_write_handler("insert", static_insert, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/bighashmap.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(NeighborDetect)
