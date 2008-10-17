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

// ALWAYS INCLUDE <click/config.h> FIRST
#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/handlercall.hh>
#include "dstclassifier.hh"

CLICK_DECLS

DstClassifier::DstClassifier() :
  _element(NULL),
  _debug(BrnLogger::DEFAULT)
{
}

DstClassifier::~DstClassifier()
{
}

int
DstClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
                  cpElement, "NodeIdentity", &_me,
                  cpElement, "Client assoc list", &_client_assoc_lst,
                  cpOptional,
                  cpHandlerName, "write handler if ethernet station", &_element, &_hname,
		  cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity element is not provided or not a NodeIdentity");

  if (!_client_assoc_lst || !_client_assoc_lst->cast("AssocList")) 
    return errh->error("AssocList element is not provided or not a AssocList");

	if ((_element != NULL) && !_element->cast("EtherSwitch"))
		return errh->error("Handler must be a handler from element EtherSwitch");
		
  return 0;
}

int
DstClassifier::initialize(ErrorHandler *)
{
  return 0;
}


void 
DstClassifier::push(int, Packet *p_in)
{

  click_ether *ether = (click_ether *)p_in->ether_header();

  if (!ether) {
    ether = (click_ether *)p_in->data();
  } else {
    BRN_DEBUG(" * using ether anno.");
  }

  EtherAddress src_addr(ether->ether_shost);
  EtherAddress dst_addr(ether->ether_dhost);

  if (_me->isIdentical(&dst_addr)) { // for me
    BRN_DEBUG(" * packet is for me.");
    output(0).push(p_in);
  } else if (_client_assoc_lst->is_associated(dst_addr)) { // for an assoc client
    BRN_DEBUG(" * packet is for an assoc client.");
    output(1).push(p_in);
  } else {
  	if (_element != NULL) {
	  	bool ethernet_station = HandlerCall::call_write(_element, _hname, dst_addr.unparse())  == 1 ? true : false;
	  	
	  	if (ethernet_station) {
	  	  BRN_DEBUG(" * packet with dst %s is for an ethernet client.", dst_addr.unparse().c_str());
	      output(2).push(p_in);
	    } else {
	      BRN_DEBUG(" * routing required; destination: %s.", dst_addr.unparse().c_str());
	      output(3).push(p_in);
	    }
  	}
  	else {
    	BRN_DEBUG(" * routing required; destination: %s.", dst_addr.unparse().c_str());
    	output(2).push(p_in);
  	}
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  DstClassifier *ds = (DstClassifier *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  DstClassifier *ds = (DstClassifier *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
DstClassifier::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(DstClassifier)

#include <click/bighashmap.cc>
 
CLICK_ENDDECLS 
