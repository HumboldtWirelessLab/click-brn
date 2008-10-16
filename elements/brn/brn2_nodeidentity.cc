#include <click/config.h>

#include "brn2_nodeidentity.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <elements/brn/standard/brnaddressinfo.hh>
#include "elements/brn/common.hh"

CLICK_DECLS

BRN2NodeIdentity::BRN2NodeIdentity()
  : _debug(BrnLogger::DEFAULT)
{
}

BRN2NodeIdentity::~BRN2NodeIdentity()
{
}

int
BRN2NodeIdentity::configure(Vector<String> &conf, ErrorHandler* errh)
{
 /* if (conf.size() != noutputs())
    return errh->error("need %d arguments, one per output port", noutputs());
*/
  int before = errh->nerrors();

  Element *e;

  for (int slot = 0; slot < conf.size(); slot++) {
    Element *e = cp_element(conf[slot], this, errh);
    BRN2Device *brn_device = (BRN2Device *)e->cast("BRN2Device");
    if (!brn_device) {
      return errh->error("element is not an BRN2Device");
    }
    else {
      click_chatter("Device: %s EtherAddress: %s Type: %s",
                    brn_device->getDeviceName().c_str(),
                    brn_device->getEtherAddress()->s().c_str(),
                    brn_device->getDeviceType().c_str());
      _node_devices.push_back(brn_device);
    }
  }

  return 0;
}

int
BRN2NodeIdentity::initialize(ErrorHandler *)
{
  return 0;
}

/* returns true if the given ethernet address belongs to this node (e.g. wlan-dev)*/
bool
BRN2NodeIdentity::isIdentical(EtherAddress *e)
{
/*  if (!_me_wlan || !_me_vlan0)
    return false;

  if ( (*e == *_me_wlan) || (*e == *_me_vlan0) ) {
    return true;
  } else {
    return false;
  }*/
  return true;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2NodeIdentity *id = (BRN2NodeIdentity *)e;
  return String(id->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRN2NodeIdentity *id = (BRN2NodeIdentity *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  id->_debug = debug;
  return 0;
}

void
BRN2NodeIdentity::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/vector.cc>
template class Vector<BRN2Device*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2NodeIdentity)