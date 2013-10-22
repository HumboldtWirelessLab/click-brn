#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <elements/brn/standard/brnaddressinfo.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#if CLICK_NS
#include <click/router.hh>
#include <click/simclick.h>
#include "txcontrol.h"
#endif

#include "brn2_device.hh"

CLICK_DECLS

BRN2Device::BRN2Device()
  : device_number(0),
    is_service_dev(false),
    is_master_dev(false),
    _routable(true),
    _allow_broadcast(true),
    _channel(0),
    no_queues(0),
    _cwmin(NULL),
    _cwmax(NULL),
    _aifs(NULL)
{
  BRNElement::init();
}

BRN2Device::~BRN2Device()
{
}

int
BRN2Device::configure(Vector<String> &conf, ErrorHandler* errh)
{
  EtherAddress me;

  String s_cwmin = "";
  String s_cwmax = "";
  String s_aifs = "";
  int v;

  if (cp_va_kparse(conf, this, errh,
      "DEVICENAME", cpkP+cpkM, cpString, &device_name,
      "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, &me,
      "DEVICETYPE", cpkP+cpkM, cpString, &device_type_string,
      "IPADDRESS", cpkP, cpIPAddress, &ipv4,
      "SERVICEDEVICE", cpkP, cpBool, &is_service_dev,
      "MASTERDEVICE", cpkP, cpBool, &is_master_dev,
      "ROUTABLE", cpkP, cpBool, &_routable,
      "ALLOW_BROADCAST", cpkP, cpBool, &_allow_broadcast,
      "CWMIN", cpkP, cpString, &s_cwmin,
      "CWMAX", cpkP, cpString, &s_cwmax,
      "AIFS", cpkP, cpString, &s_aifs,
      cpEnd) < 0)
    return -1;

  unsigned char en[6];
  bool val;

  if ( ( device_type_string != STRING_WIRELESS ) &&
        ( device_type_string != STRING_WIRED ) &&
        ( device_type_string != STRING_VIRTUAL ) )
    return errh->error("Unsupported devicetype");

  device_type = getTypeIntByString(device_type_string);

  if( EtherAddress() != me ) {
    device_etheraddress = me;
  } else {
    val = BRNAddressInfo::query_ethernet(device_name + ":eth", en, this);
    if (val) {
      device_etheraddress = EtherAddress(en);
      BRN_DEBUG(" * ether address of device : %s", device_etheraddress.unparse().c_str());
    }
  }

  device_etheraddress_fix = device_etheraddress;

  Vector<String> args;
  cp_spacevec(s_cwmin, args);
  no_queues = args.size();
  if ( no_queues > 0 ) {
    _cwmin = new uint16_t[no_queues];
    _cwmax = new uint16_t[no_queues];
    _aifs = new uint16_t[no_queues];

//TODO: Better check for params. Better Error handling."
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmin[i] = v;
    }
    args.clear();
    cp_spacevec(s_cwmax, args);
    if ( args.size() < no_queues ) no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmax[i] = v;
    }
    args.clear();
    cp_spacevec(s_aifs, args);
    if ( args.size() < no_queues ) no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _aifs[i] = v;
    }
    args.clear();
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

const EtherAddress *
BRN2Device::getEtherAddress()
{
  return &device_etheraddress;
}

void
BRN2Device::setEtherAddress(EtherAddress *ea)
{
  device_etheraddress = *ea;
}

EtherAddress *
BRN2Device::getEtherAddressFix()
{
  return &device_etheraddress_fix;
}

const IPAddress *
BRN2Device::getIPAddress()
{
  return &ipv4;
}

void
BRN2Device::setIPAddress(IPAddress *ip)
{
  ipv4 = *ip;
}

#ifdef HAVE_IP6
const IP6Address *
BRN2Device::getIP6Address()
{
  return &ipv6;
}

void
BRN2Device::setIP6Address(IP6Address *ip)
{
  ipv6 = *ip;
}
#endif

uint32_t
BRN2Device::getDeviceType()
{
  return device_type;
}

void
BRN2Device::setDeviceType(uint32_t type)
{
  device_type = type;
  device_type_string = getTypeStringByInt(type);
}

const String&
BRN2Device::getDeviceTypeString()
{
  return device_type_string;
}

void
BRN2Device::setDeviceTypeString(String type)
{
  device_type_string = type;
  device_type = getTypeIntByString(type);
}

void
BRN2Device::setDeviceNumber(uint8_t number)
{
  device_number = number;
}

uint8_t
BRN2Device::getDeviceNumber()
{
  return device_number;
}


bool
BRN2Device::is_service_device()
{
  return is_service_dev;
}

bool
BRN2Device::is_master_device()
{
  return is_master_dev;
}

bool
BRN2Device::is_routable()
{
  return _routable;
}

void
BRN2Device::set_routable(bool routable)
{
  _routable = routable;
}

#if CLICK_NS
void
BRN2Device::abort_transmission(EtherAddress &dst)
{
  struct tx_control_header txch;

  txch.operation = TX_ABORT;
  txch.flags = 0;
  memcpy(txch.dst_ea, dst.data(), 6);

  BRN_ERROR("Abort tx: %s", dst.unparse().c_str());
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_TX_CONTROL, &txch);

  if ( txch.flags != 0 ) BRN_ERROR("TXCtrl-Error");
#else
void
BRN2Device::abort_transmission(EtherAddress &)
{
#endif
}

uint32_t
BRN2Device::getTypeIntByString(String type)
{
  if ( type.compare(STRING_WIRED, strlen(STRING_WIRED)) == 0 ) return DEVICETYPE_WIRED;
  if ( type.compare(STRING_WIRELESS, strlen(STRING_WIRELESS)) == 0 ) return DEVICETYPE_WIRELESS;
  if ( type.compare(STRING_VIRTUAL, strlen(STRING_VIRTUAL)) == 0 ) return DEVICETYPE_VIRTUAL;

  return DEVICETYPE_UNKNOWN;
}

String
BRN2Device::getTypeStringByInt(uint32_t type)
{
  switch (type) {
    case DEVICETYPE_WIRED: return String(STRING_WIRED, strlen(STRING_WIRED));
    case DEVICETYPE_WIRELESS: return String(STRING_WIRELESS, strlen(STRING_WIRELESS));
    case DEVICETYPE_VIRTUAL: return String(STRING_VIRTUAL, strlen(STRING_VIRTUAL));
  }

  return String(STRING_UNKNOWN, strlen(STRING_UNKNOWN));
}

String
BRN2Device::device_info()
{
  StringAccum sa;
  sa << "<device node=\"" << BRN_NODE_NAME << "\" name=\"" << getDeviceName() << "\" address=\"" << getEtherAddress()->unparse();
  sa << "\" fix_address=\"" << getEtherAddressFix()->unparse() << "\" type=\"" << getDeviceTypeString() << "\" >\n";
  sa << "\t<queues count=\"" << (uint32_t)no_queues << "\" >\n";

  for ( uint32_t i = 0; i < no_queues; i++ )
    sa << "\t\t<queue index=\"" << i << "\" cwmin=\"" << (uint32_t)_cwmin[i] << "\" cwmax=\"" << (uint32_t)_cwmax[i] << "\" aifs=\"" << (uint32_t)_aifs[i] << "\" />\n";

  sa << "\t</queues>\n</device>\n";

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_device_info(Element *e, void *)
{
  BRN2Device *dev = (BRN2Device *)e;
  return dev->device_info();
}

static String
read_device_address(Element *e, void *)
{
  BRN2Device *dev = (BRN2Device *)e;
  return dev->getEtherAddress()->unparse();
}

static int
write_address(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BRN2Device *dev = (BRN2Device *)e;

  String s = cp_uncomment(in_s);
  EtherAddress new_ea;
  if (!cp_ethernet_address(s, &new_ea))
    return errh->error("address parameter must be EtherAddress");

  dev->setEtherAddress(&new_ea);

  return 0;
}

static int
write_reset_address(const String &/*in_s*/, Element *e, void *, ErrorHandler */*errh*/)
{
  BRN2Device *dev = (BRN2Device *)e;
  dev->setEtherAddress(dev->getEtherAddressFix());
  return 0;
}

void
BRN2Device::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("deviceinfo", read_device_info, 0);
  add_read_handler("address", read_device_address, 0);

  add_write_handler("reset_address", write_reset_address, 0);
  add_write_handler("address", write_address, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Device)
