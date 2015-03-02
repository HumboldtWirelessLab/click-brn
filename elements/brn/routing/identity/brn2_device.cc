#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>

#include <elements/brn/standard/brnaddressinfo.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#if CLICK_NS
#include <click/router.hh>
#include <click/simclick.h>
#include "rxtxcontrol.h"
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
    _no_queues(0),
    _cwmin(NULL),
    _cwmax(NULL),
    _aifs(NULL),
    _queue_info(NULL)
{
  BRNElement::init();
}

BRN2Device::~BRN2Device()
{
  if ( _queue_info != NULL ) {
    delete[] _queue_info;
    _queue_info = NULL;
  }
}

int
BRN2Device::configure(Vector<String> &conf, ErrorHandler* errh)
{
  EtherAddress me;

  if (cp_va_kparse(conf, this, errh,
      "DEVICENAME", cpkP+cpkM, cpString, &device_name,
      "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, &me,
      "DEVICETYPE", cpkP+cpkM, cpString, &device_type_string,
      "IPADDRESS", cpkP, cpIPAddress, &ipv4,
      "SERVICEDEVICE", cpkP, cpBool, &is_service_dev,
      "MASTERDEVICE", cpkP, cpBool, &is_master_dev,
      "ROUTABLE", cpkP, cpBool, &_routable,
      "ALLOW_BROADCAST", cpkP, cpBool, &_allow_broadcast,
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

  return 0;
}

int
BRN2Device::initialize(ErrorHandler *)
{
  get_cca();

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

/********************************************************************************************/
/********************************************************************************************/
/********************************************************************************************/

void
BRN2Device::get_cca()
{
  int cca[4];
  cca[0] = 0; //command read
  cca[1] = 0;
  cca[2] = 0;
  cca[3] = 0;

#if CLICK_NS
  simclick_sim_command(router()->simnode(), SIMCLICK_CCA_OPERATION, &cca);
#endif

  _rx_threshold = cca[1];
  _cs_threshold = cca[2];
  _cp_threshold = cca[3];
}

void
BRN2Device::set_cca(int cs_threshold, int rx_threshold, int cp_threshold)
{

  if ( cs_threshold != 0 ) _cs_threshold = cs_threshold;
  if ( rx_threshold != 0 ) _rx_threshold = rx_threshold;
  if ( cp_threshold != 0 ) _cp_threshold = cp_threshold;

#if CLICK_NS
  int cca[4];
  cca[0] = 1; //command set
  cca[1] = _cs_threshold;
  cca[2] = _rx_threshold;
  cca[3] = _cp_threshold;

  simclick_sim_command(router()->simnode(), SIMCLICK_CCA_OPERATION, &cca);
#else
  (void)cs_threshold;
  (void)rx_threshold;
  (void)cp_threshold;
#endif
}

uint32_t
BRN2Device::set_backoff()
{
#if CLICK_NS
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_SET_BACKOFF, _queue_info);
#endif

  return 0;
}

uint32_t
BRN2Device::get_backoff()
{
#if CLICK_NS
  if ( _no_queues == 0 ) {
    int boq_info[2];
    boq_info[0] = 0;
    simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_GET_BACKOFF, boq_info);
    _no_queues = boq_info[1];
    if ( _no_queues > 0 ) {
      _queue_info = new uint32_t[2 + 3 * _no_queues];
      memset(_queue_info,0, (2 + 3 * _no_queues)*sizeof(uint32_t));
      _cwmin = &_queue_info[2];
      _cwmax = &_queue_info[2+_no_queues];
      _aifs = &_queue_info[2 + (2 * _no_queues)];
    }
  }

  _queue_info[0] = _no_queues;
  _queue_info[1] = 0;

  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_GET_BACKOFF, _queue_info);
#endif
  return 0;
}

int
BRN2Device::set_backoff_scheme(uint32_t scheme)
{
#if CLICK_NS
  struct tx_control_header txch;

  txch.operation = SET_BACKOFFSCHEME;
  txch.flags = 0;
  txch.bo_scheme = scheme;

  BRN_DEBUG("Set backoff scheme: %d", scheme);
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_TX_CONTROL, &txch);

  if ( txch.flags != 0 ) {
    BRN_ERROR("TXCtrl-Error");
    return 1;
  }
#else
  (void)scheme;
#endif
  return 0;
}

int
BRN2Device::abort_transmission(EtherAddress &dst)
{
#if CLICK_NS
  struct tx_control_header txch;

  txch.operation = TX_ABORT;
  txch.flags = 0;
  memcpy(txch.dst_ea, dst.data(), 6);

  BRN_DEBUG("Abort tx: %s", dst.unparse().c_str());
  simclick_sim_command(router()->simnode(), SIMCLICK_WIFI_TX_CONTROL, &txch);

  if ( txch.flags != 0 ) {
    BRN_ERROR("TXCtrl-Error");
    return 1;
  }
#else
  (void)dst;
#endif
  return 0;
}

int
BRN2Device::set_power_iwconfig(int power, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  cmda << " " << device_name;
  cmda << " txpower " << power;
  String cmd = cmda.take_string();

  click_chatter("SetPower command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", errh);
  if (out) click_chatter("%s: %s", cmd.c_str(), out.c_str());
#else
#if CLICK_NS

#endif
#endif

  _power = power;

  return 0;
}

String
BRN2Device::device_info()
{
  StringAccum sa;
  sa << "<device node=\"" << BRN_NODE_NAME << "\" name=\"" << getDeviceName() << "\" address=\"" << getEtherAddress()->unparse();
  sa << "\" fix_address=\"" << getEtherAddressFix()->unparse() << "\" type=\"" << getDeviceTypeString() << "\" >\n";
  sa << "\t<config power=\"" << _power << "\" >\n";
  sa << "\t\t<queues count=\"" << (uint32_t)_no_queues << "\" >\n";

  for ( uint32_t i = 0; i < _no_queues; i++ )
    sa << "\t\t\t<queue index=\"" << i << "\" cwmin=\"" << (uint32_t)_cwmin[i] << "\" cwmax=\"" << (uint32_t)_cwmax[i] << "\" aifs=\"" << (uint32_t)_aifs[i] << "\" />\n";

  sa << "\t\t</queues>\n\t\t<cca rxthreshold=\"" << _rx_threshold << "\" csthreshold=\"" << _cs_threshold << "\" capture=\"" << _cp_threshold << "\"/>\n\t</config>\n</device>\n";

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

static int
write_power(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2Device *f = (BRN2Device *)e;
  String s = cp_uncomment(in_s);
  uint32_t power;

  if (!cp_integer(s, &power))
   return errh->error("power parameter must be integer");

  f->set_power_iwconfig(power, errh);

  return 0;
}

static String
read_power(Element *e, void */*thunk*/)
{
  BRN2Device *dev = (BRN2Device *)e;
  return String(dev->get_power());
}

static int
write_cca(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2Device *f = (BRN2Device *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  int cs,rx,cp;
  if (!cp_integer(args[0], &cs)) return errh->error("cs parameter must be integer");
  if (!cp_integer(args[1], &rx)) return errh->error("rx parameter must be integer");
  if (!cp_integer(args[2], &cp)) return errh->error("cp parameter must be integer");

  f->set_cca(cs,rx,cp);

  return 0;
}

void
BRN2Device::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("deviceinfo", read_device_info, 0);
  add_read_handler("address", read_device_address, 0);
  add_read_handler("power", read_power, 0);

  add_write_handler("reset_address", write_reset_address, 0);
  add_write_handler("address", write_address, 0);
  add_write_handler("power", write_power, 0);
  add_write_handler("cca", write_cca, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Device)
