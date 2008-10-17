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
 * onehopneighbor.{cc,hh} -- checks if destination is 1-hop-reachable
 * A. Zubow
 */

#include <click/config.h>

#include "onehopneighbor.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
CLICK_DECLS

/**
  * @deprecated use DstClassifier
  */
OneHopNeighbor::OneHopNeighbor()
  : _debug(BrnLogger::DEFAULT),
    _me(),
    _client_assoc_lst()
{
}

OneHopNeighbor::~OneHopNeighbor()
{
}

int
OneHopNeighbor::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
		  cpOptional,
		  cpElement, "NodeIdentity", &_me,
      cpElement, "Client assoc list", &_client_assoc_lst,
		  cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_client_assoc_lst || !_client_assoc_lst->cast("AssocList")) 
    return errh->error("AssocList not specified");

  return 0;
}

int
OneHopNeighbor::initialize(ErrorHandler *)
{
  return 0;
}

/* displays the content of a brn packet */
void
OneHopNeighbor::push(int, Packet *p_in)
{
  BRN_DEBUG(" * push called()");

  click_ether *ether = (click_ether *)p_in->ether_header();

  if (!ether) {
    ether = (click_ether *)p_in->data();
  } else {
    BRN_DEBUG(" * using ether anno.");
  }

  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  BRN_DEBUG(" * got Ethernet packet (without BRN) with destination is %s", dst_addr.unparse().c_str());

  if (!(_client_assoc_lst->is_associated(src_addr) || _me->isIdentical(&src_addr))) { // sender is not associated with us
    BRN_DEBUG(" * sender is not associated with us; drop packet.");
    p_in->kill();
    return;
  }

  //packet is for me
  if (_me->isIdentical(&dst_addr) ) {
    BRN_DEBUG(" * packet is for me; process.");
    output(2).push(p_in);
    return;
  }

  //destination (client) is associated with me
  if (_client_assoc_lst->is_associated(dst_addr)) {
    BRN_DEBUG(" * the destination (client) is associated with me.");
    output(0).push(p_in);
    return;
  }

  //destination is an internal node
  BRN_DEBUG(" * packet has to be forwarded.");
  output(1).push(p_in);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  OneHopNeighbor *on = (OneHopNeighbor *)e;
  return String(on->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  OneHopNeighbor *on = (OneHopNeighbor *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  on->_debug = debug;
  return 0;
}

void
OneHopNeighbor::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OneHopNeighbor)
