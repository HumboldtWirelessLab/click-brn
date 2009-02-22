#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/vector.hh>
#include "vlannodemap.hh"


CLICK_DECLS

VlanNodeMap::VlanNodeMap()
{
}

VlanNodeMap::~VlanNodeMap()
{
}

int
VlanNodeMap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;

  if (cp_va_kparse(conf, this, errh,
     "DEBUG", cpkP, cpBool, &_debug,
      cpEnd) < 0)
    return -1;


  return 0;
}

void
VlanNodeMap:: inser(EtherAddress,uint8_t) {
  
}

uint8_t
VlanNodeMap::getVlan(EtherAddress) {
  
}

enum {
  H_DEBUG,
  H_READ};

static String
VlanNodeMap_read_param(Element *e, void *thunk)
{
  VlanNodeMap *vnm = (VlanNodeMap *)e;
  switch ((uintptr_t) thunk) {
    case H_DEBUG: {
      return String(vnm->_debug) + "\n";
    }
   default:
    return String();
  }
}

static int
VlanNodeMap_write_param(const String &in_s, Element *e, void *vparam,
                                 ErrorHandler *errh)
{
  VlanNodeMap *f = (VlanNodeMap *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_DEBUG: {
      bool debug;
      if (!cp_bool(s, &debug))
        return errh->error("debug parameter must be boolean");
      f->_debug = debug;
      break;
    }
  }

  return 0;
}

void
VlanNodeMap::add_handlers()
{
  add_read_handler("debug", VlanNodeMap_read_param, (void *) H_DEBUG);

  add_write_handler("debug", VlanNodeMap_write_param, (void *) H_DEBUG);
}

#include <click/vector.cc>
template class Vector<VlanNodeMap::VlanNode>;

CLICK_ENDDECLS
EXPORT_ELEMENT(VlanNodeMap)

