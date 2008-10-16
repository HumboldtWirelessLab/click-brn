#include <click/config.h>

#include "brn2_device.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/common.hh"
#include <elements/brn/standard/brnaddressinfo.hh>

CLICK_DECLS

BRN2Device::BRN2Device()
  : _debug(BrnLogger::DEFAULT)
{
}

BRN2Device::~BRN2Device()
{
}

int
BRN2Device::configure(Vector<String> &conf, ErrorHandler* errh)
{
  EtherAddress me;

  if (cp_va_parse(conf, this, errh,
      cpOptional,
      cpKeywords,
      "DEVICENAME", cpString, "Devicename", &_dev_info.device_name,
      "ETHERADDRESS", cpEtherAddress, "Deviceaddress", &me,
      "DEVICETYPE", cpString, "Devicetype", &_dev_info.device_type,
      cpEnd) < 0)
    return -1;

  unsigned char en[6];
  bool val;

  if( EtherAddress() != me ) {
    _dev_info.device_etheraddress = new EtherAddress(me);
  } else {
    val = BRNAddressInfo::query_ethernet(_dev_info.device_name + ":eth", en, this);
    if (val) {
      _dev_info.device_etheraddress = new EtherAddress(en);
      BRN_DEBUG(" * ether address of device : %s", _dev_info.device_etheraddress->s().c_str());
    }
  }

  return 0;
}

int
BRN2Device::initialize(ErrorHandler *)
{
  return 0;
}

const String&
BRN2Device::getDeviceName()
{
  return _dev_info.device_name;
}

void
BRN2Device::setDeviceName(String dev_name)
{
  _dev_info.device_name = dev_name;
}

EtherAddress *
BRN2Device::getEtherAddress()
{
  return _dev_info.device_etheraddress;
}

const String&
BRN2Device::getDeviceType()
{
  return _dev_info.device_type;
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

static String
read_device_info(Element *e, void *)
{
  BRN2Device *dev = (BRN2Device *)e;
  return "Device: " + dev->getDeviceName() +
         "\nEtherAddress: " + dev->getEtherAddress()->s() +
         "\nType: " + dev->getDeviceType() + "\n";
}


void
BRN2Device::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
  add_read_handler("deviceinfo", read_device_info, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Device)
