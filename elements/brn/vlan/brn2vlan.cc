#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include "brn2vlan.hh"
#include "../wifi/brn2_wirelessinfolist.hh"
#include "../services/dhcp/dhcpsubnetlist.hh"
#include "elements/brn/vlan/brn2vlantable.hh"

CLICK_DECLS

BRN2VLAN::BRN2VLAN()
 : _wifiinfolist(NULL),_dhcpsubnetlist(NULL),_vlantable(NULL),_debug(0)
{
}

BRN2VLAN::~BRN2VLAN()
{
}

int
BRN2VLAN::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;

  if (cp_va_kparse(conf, this, errh,
      "WIRELESSINFOLIST", cpkP+cpkM, cpElement, &_wifiinfolist,
      "DHCPSUBNETLIST", cpkP+cpkM, cpElement, &_dhcpsubnetlist,
      "VLANTABLE", cpkP+cpkM, cpElement, &_vlantable,            //remove node of VLAN in the table, if vlan is removed
      "DEBUG", cpkP, cpBool, &_debug,
      cpEnd) < 0)
    return -1;


  return 0;
}

enum {
  H_DEBUG,
  H_READ,
  H_INSERT,
  H_REMOVE };

static String
BRN2VLAN_read_param(Element *e, void *thunk)
{
  BRN2VLAN *wil = reinterpret_cast<BRN2VLAN *>(e);
  switch ((uintptr_t) thunk) {
    case H_DEBUG: {
      return String(wil->_debug) + "\n";
    }
    case H_READ: {
      StringAccum sa;
      return sa.take_string();
   }
   default:
    return String();
  }
}

static int
BRN2VLAN_write_param(const String &in_s, Element *e, void *vparam,
                                 ErrorHandler *errh)
{
  BRN2VLAN *f = reinterpret_cast<BRN2VLAN *>(e);
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
      break;
    }
  }

  return 0;
}

void
BRN2VLAN::add_handlers()
{
  add_read_handler("debug", BRN2VLAN_read_param, (void *) H_DEBUG);
  add_read_handler("info", BRN2VLAN_read_param, (void *) H_READ);

  add_write_handler("debug", BRN2VLAN_write_param, (void *) H_DEBUG);
  add_write_handler("insert", BRN2VLAN_write_param, (void *) H_INSERT);
}

#include <click/vector.cc>
template class Vector<BRN2VLAN::VlanInfo>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2VLAN)

