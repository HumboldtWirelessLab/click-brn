#include <click/config.h>

#include "brn2_device.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include <click/standard/addressinfo.hh>

CLICK_DECLS

BRN2Device::BRN2Device()
  : _debug(BrnLogger::DEFAULT),
    _ether_address( NULL )
{
}

BRN2Device::~BRN2Device()
{
  delete _ether_address;
  _ether_address = NULL;
}

int
BRN2Device::configure(Vector<String> &conf, ErrorHandler* errh)
{
  EtherAddress me;

  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpKeywords,
      "DEVICENAME", cpString, "Devicename", &_dev_name,
      "ETHERADDRESS", cpEtherAddress, "Deviceaddress", &_dev_ether_address,
      "DEVICETYPE", cpString, "Devicetype", &_dev_type,
      cpEnd) < 0)
    return -1;

  unsigned char en[6];
  bool val;

  if( EtherAddress() != me ) {
    _dev_ether_address = new EtherAddress(me);
  } else {
    val = AddressInfo::query_ethernet(_dev_name + ":eth", en, this);
    if (val) {
      _dev_ether_address = new EtherAddress(en);
      BRN_DEBUG(" * ether address of device : %s", _dev_ether_address->s().c_str());
    }
  }
}


int
BRN2Device::initialize(ErrorHandler *)
{
  return 0;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRN2Device *id = (BRN2Device *)e;
  return String(id->_debug) + "\n";
}

static int
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BRN2Device *id = (BRN2Device *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug))
    return errh->error("debug parameter must be an integer value between 0 and 4");
  id->_debug = debug;
  return 0;
}

void
BRN2Device::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Device)
