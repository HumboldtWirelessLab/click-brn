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
 * identityclassifier.{cc,hh}
 * A. Zubow
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "identityclassifier.hh"

CLICK_DECLS

IdentityClassifier::IdentityClassifier()
  : _debug(BrnLogger::DEFAULT)
{
}

IdentityClassifier::~IdentityClassifier()
{
}

int
IdentityClassifier::configure(Vector<String> &conf, ErrorHandler* errh)
{
  bool drop_own = false, drop_other = true;
  if (cp_va_kparse(conf, this, errh,
      cpElement, "NodeIdentity", &_id,
      cpOptional,
      cpBool, "Drop packets from us?", &drop_own,
      cpBool, "Drop packets to others?", &drop_other,
      cpKeywords,
      "DROP_OWN", cpBool, "Drop packets from us?", &drop_own,
      "DROP_OTHER", cpBool, "Drop packets to others?", &drop_other,
      "DEBUG", cpInteger, "Debug output?", &_debug,
      cpEnd) < 0)
    return -1;
  _drop_own = drop_own;
  _drop_other = drop_other;

  if (!_id || !_id->cast("NodeIdentity")) 
    return errh->error("NodeIdentity element is not provided or not a NodeIdentity");

  return 0;
}

int
IdentityClassifier::initialize(ErrorHandler *)
{
  return 0;
}

/* displays the content of a brn packet */
Packet *
IdentityClassifier::simple_action(Packet *p_in)
{
  BRN_DEBUG(" * IdentityClassifier::push called()");

  // prefer ether annotation over ethernet header
  click_ether *ether = (click_ether *) p_in->ether_header();

  if (!ether) {
    ether = (click_ether *) p_in->data();
  } else {
    BRN_DEBUG(" * using ether anno.");
  }

  // get src and dst mac 
  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  if (_drop_own && _id->isIdentical(&src_addr)) 
  {
    BRN_DEBUG(" *  filter packet from %s to %s", src_addr.unparse().c_str(), dst_addr.unparse().c_str());

    checked_output_push(1, p_in);
    return NULL;
  }
  else if (_id->isIdentical(&dst_addr))
  {
    p_in->set_packet_type_anno(Packet::HOST);
    return p_in;
  } 
  else if (dst_addr.is_broadcast())
  {
    p_in->set_packet_type_anno(Packet::BROADCAST);
    return p_in;
  } 
  else if (dst_addr.is_group()) 
  {
    p_in->set_packet_type_anno(Packet::MULTICAST);
    return p_in;
  } 

  p_in->set_packet_type_anno(Packet::OTHERHOST);
  if (!_drop_other)
    return (p_in);

  BRN_DEBUG(" * filter packet from %s to %s", src_addr.unparse().c_str(), dst_addr.unparse().c_str());

  checked_output_push(1, p_in);
  return (NULL);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  IdentityClassifier *on = (IdentityClassifier *)e;
  return String(on->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  IdentityClassifier *on = (IdentityClassifier *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be boolean");
  on->_debug = debug;
  return 0;
}

void
IdentityClassifier::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IdentityClassifier)
