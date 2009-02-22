#include <click/config.h>

#include "brn2_device.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/common.hh"
#include <elements/brn2/standard/brnaddressinfo.hh>

CLICK_DECLS

BRN2Device::BRN2Device()
  : _debug(BrnLogger::DEFAULT),
    device_number(0)
{
}

BRN2Device::~BRN2Device()
{
}

int
BRN2Device::configure(Vector<String> &conf, ErrorHandler* errh)
{
  EtherAddress me;

  if (cp_va_kparse(conf, this, errh,
      "DEVICENAME", cpkP+cpkM, cpString, &device_name,
      "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, &me,
      "DEVICETYPE", cpkP+cpkM, cpString, &device_type,
      cpEnd) < 0)
    return -1;

  unsigned char en[6];
  bool val;

  if ( ( device_type != WIRELESS ) && ( device_type != WIRED ) )
    return errh->error("Unsupported devicetype");

  if( EtherAddress() != me ) {
    device_etheraddress = new EtherAddress(me);
  } else {
    val = BRNAddressInfo::query_ethernet(device_name + ":eth", en, this);
    if (val) {
      device_etheraddress = new EtherAddress(en);
      BRN_DEBUG(" * ether address of device : %s", device_etheraddress->unparse().c_str());
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
  return device_name;
}

void
BRN2Device::setDeviceName(String dev_name)
{
  device_name = dev_name;
}

EtherAddress *
BRN2Device::getEtherAddress()
{
  return device_etheraddress;
}

const String&
BRN2Device::getDeviceType()
{
  return device_type;
}

void
BRN2Device::setDeviceNumber(uint8_t number) {
  device_number = number;
}

uint8_t
BRN2Device::getDeviceNumber() {
  return device_number;
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
         "\nEtherAddress: " + dev->getEtherAddress()->unparse() +
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
