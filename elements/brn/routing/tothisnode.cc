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
 * tothisnode.{cc,hh}
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "tothisnode.hh"

CLICK_DECLS

ToThisNode::ToThisNode()
  : _debug(BrnLogger::DEFAULT)
{
}

ToThisNode::~ToThisNode()
{
}

int
ToThisNode::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
		  cpOptional,
		  cpElement, "NodeIdentity", &_id,
		  cpEnd) < 0)
    return -1;

  if (!_id || !_id->cast("NodeIdentity"))
    return errh->error("NodeIdentity not specified");

  return 0;
}

int
ToThisNode::initialize(ErrorHandler *)
{
  return 0;
}

/* displays the content of a brn packet */
void
ToThisNode::push(int, Packet *p_in)
{
  BRN_DEBUG(" * ToThisNode::push called()");

  // use ether annotation
  click_ether *ether = (click_ether *) p_in->ether_header();

  if (!ether)
  {
    BRN_DEBUG("no ether header found -> killing packet.");
    p_in->kill();
    return;
  }

  // get src and dst mac 
  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);


  if (_id->isIdentical(&dst_addr) || dst_addr.is_broadcast()) {
    // packet is for me
    BRN_DEBUG(" * packet is sent to this node (output port 0)");
    output(0).push(p_in);
    return;
  }

  // packet is not for me
  output(1).push(p_in);

}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  ToThisNode *on = (ToThisNode *)e;
  return String(on->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  ToThisNode *on = (ToThisNode *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  on->_debug = debug;
  return 0;
}

void
ToThisNode::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ToThisNode)
