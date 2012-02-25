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
#include "brn2vlantable.hh"
#include <click/straccum.hh>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
CLICK_DECLS


BRN2VLANTable::BRN2VLANTable()
{
}

BRN2VLANTable::~BRN2VLANTable()
{
}

int
BRN2VLANTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
  (void) conf;
  (void) errh;

  return 0;
}

String
BRN2VLANTable::all_vlans() {

  StringAccum sa;
  for (VlanTable::const_iterator i = _vlans.begin(); i.live(); i++) {
    uint16_t vlanid = i.value();
    EtherAddress ea = i.key();
    sa << this << " eth " << ea.unparse().c_str() << " vlanid " << (int) vlanid << "\n";
  }

  return sa.take_string();
    }

enum {
  H_DEBUG,
  H_READ,
  H_INSERT,
  H_REMOVE };

static String
BRN2VLANTable_read_param(Element *e, void *thunk)
{
  BRN2VLANTable *vt = (BRN2VLANTable *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG: {
      return String(vt->_debug) + "\n";
    }
    case H_READ: {
      return vt->all_vlans();
    }
    default:
      return String();
  }
}

static int
BRN2VLANTable_write_param(const String &in_s, Element *e, void *vparam,
                                      ErrorHandler *errh)
{
  BRN2VLANTable *f = (BRN2VLANTable *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
    case H_INSERT: {
      Vector<String> args;
      cp_spacevec(s, args);

      EtherAddress _ea;
      uint32_t _vlan;
      cp_ethernet_address(args[0], &_ea);
      cp_integer(args[1], &_vlan);

      f->insert(_ea, _vlan);
    }
  }

  return 0;
}

void
BRN2VLANTable::add_handlers()
{
  add_read_handler("debug", BRN2VLANTable_read_param, (void *) H_DEBUG);
  add_read_handler("vlan", BRN2VLANTable_read_param, (void *) H_READ);

  add_write_handler("debug", BRN2VLANTable_write_param, (void *) H_DEBUG);
  add_write_handler("insert", BRN2VLANTable_write_param, (void *) H_INSERT);
}


EXPORT_ELEMENT(BRN2VLANTable)
#include <click/bighashmap.cc>
CLICK_ENDDECLS
