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
 * clientds.{cc,hh} -- forwards client packets to the right device
 * A. Zubow
 */

#include <click/config.h>
#include "elements/brn/common.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "clientds.hh"

CLICK_DECLS

ClientDS::ClientDS()
  : _debug(BrnLogger::DEFAULT)
{
}

ClientDS::~ClientDS()
{
}

int
ClientDS::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpString, "#1 device (wlan)", &_dev_wlan_name,
      cpString, "#2 device (vlan1)", &_dev_vlan1_name,
      cpString, "#3 device (vlan2)", &_dev_vlan2_name,
      cpString, "#4 device (debug)", &_dev_vlan_debug_name,
      cpElement, "Client assoc list", &_client_assoc_lst,
      cpEnd) < 0)
    return -1;

  if (!_client_assoc_lst || !_client_assoc_lst->cast("AssocList")) 
    return errh->error("AssocList element is not provided or not a AssocList");

  return 0;
}

int
ClientDS::initialize(ErrorHandler *)
{
  return 0;
}

void
ClientDS::uninitialize()
{
  //cleanup
}

/* Processes an incoming (brn-)packet. */
void
ClientDS::push(int, Packet *p_in)
{
  BRN_DEBUG(" * push()");

  click_ether *ether = (click_ether *)p_in->data();

  EtherAddress dst(ether->ether_dhost);

  //estimate the right device
  String device(_client_assoc_lst->get_device_name(dst));

  int out_port = -1;
  if (device == _dev_wlan_name) {
    out_port = 0;
  } else if (device == _dev_vlan1_name) {
    out_port = 1;
  } else if (device == _dev_vlan2_name) {
    out_port = 2;
  } else if (device == _dev_vlan_debug_name) {
    out_port = 3;
  } else {
    BRN_WARN("Unknown client %s (%s); drop packet.", dst.unparse().c_str(), device.c_str());
    p_in->kill();
    return;
  }
  output(out_port).push(p_in);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  ClientDS *ds = (ClientDS *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  ClientDS *ds = (ClientDS *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
ClientDS::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ClientDS)
