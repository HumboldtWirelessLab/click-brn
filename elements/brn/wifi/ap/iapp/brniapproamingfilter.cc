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
 * brniapproamingfilter.{cc,hh} -- filters out packets to disassociated STA's to output 1
 * M. Kurth
 */
 
#include <click/config.h>
#include <clicknet/wifi.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <elements/wifi/wirelessinfo.hh>

#include "elements/brn/wifi/ap/brn2_assoclist.hh"
#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brniapproamingfilter.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

BrnIappRoamingFilter::BrnIappRoamingFilter() :
  _debug(BrnLogger::DEFAULT)
{
}

////////////////////////////////////////////////////////////////////////////////

BrnIappRoamingFilter::~BrnIappRoamingFilter()
{
}

////////////////////////////////////////////////////////////////////////////////

int 
BrnIappRoamingFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  UNREFERENCED_PARAMETER(errh);
  
  if (cp_va_kparse(conf, this, errh,
      "ASSOCLIST", cpkP+cpkM, cpElement,/* "AssocList",*/ &_assoc_list,
      "DEBUG", cpkP, cpInteger, /*"Debug",*/ &_debug,
      cpEnd) < 0)
    return -1;
  
/*RobAt  if (!_assoc_list && _assoc_list->cast("AssocList") == 0)
    return errh->error("ASSOCLIST element is not a AssocList");
*/
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

int
BrnIappRoamingFilter::initialize(ErrorHandler *errh)
{
  if (input_is_pull(0) != output_is_pull(0))
    errh->error("BrnIappRoamingFilter: Input 0 must have the processing of output 0.");
  
  if (noutputs() > 1 && !output_is_push(1))
    errh->error("BrnIappRoamingFilter: Output 0 must be push.");

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

Packet *
BrnIappRoamingFilter::pull(int port)
{
  UNREFERENCED_PARAMETER(port);
  
  Packet *p = NULL;
  
  while (NULL == p)
  {
    p = input(0).pull();
    if (NULL == p)
      return (NULL);
  
    struct click_wifi *wifi = (struct click_wifi *) p->data();
    assert (NULL != wifi);
    if (!wifi) {
      BRN_WARN("got packet without valid wifi!");
      return (p);
    }
  
    if (WIFI_FC0_VERSION_0 != (wifi->i_fc[0] & WIFI_FC0_VERSION_MASK)
      || WIFI_FC0_TYPE_DATA != (wifi->i_fc[0] & WIFI_FC0_TYPE_MASK)
      || WIFI_FC1_DIR_FROMDS != (wifi->i_fc[1] & WIFI_FC1_DIR_MASK))
      return (p);
  
    // Check if this client moved away
    EtherAddress ether_dst(wifi->i_addr1);
/*RobAt    if (NULL != _assoc_list && _assoc_list->is_roaming(ether_dst)) {
      BRN_DEBUG("filtered packet to roamed sta %s", ether_dst.unparse().c_str());
      
      checked_output_push(1, p);
      p = NULL;
    }*/
  }
    
  return p;
}

////////////////////////////////////////////////////////////////////////////////

enum {H_DEBUG};

static String 
read_param(Element *e, void *thunk)
{
  BrnIappRoamingFilter *td = (BrnIappRoamingFilter *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(td->_debug) + "\n";
  default:
    return String();
  }
}

static int 
write_param(const String &in_s, Element *e, void *vparam,
          ErrorHandler *errh)
{
  BrnIappRoamingFilter *f = (BrnIappRoamingFilter *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void 
BrnIappRoamingFilter::add_handlers()
{
  add_read_handler("debug", read_param, (void *) H_DEBUG);
  add_write_handler("debug", write_param, (void *) H_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnIappRoamingFilter)

////////////////////////////////////////////////////////////////////////////////
