#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <click/standard/addressinfo.hh>
#include <unistd.h>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#ifdef CLICK_NS
#include <click/router.hh>
#include <click/simclick.h>
#include "rxtxcontrol.h"
#endif

#include "brn2_device.hh"

#include "elements/brn/wifi/config/wificonfig_iwlib.hh"
#include "elements/brn/wifi/config/wificonfig_sim.hh"


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
    _queue_info(NULL),
    _power(0),
    _wireless_device_config_string(""),
    _wireless_availablerates(NULL),
    _wireless_availablechannels(NULL),
    _wificonfig(NULL)
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
      "WIRELESSCONFIG", cpkP, cpString, &_wireless_device_config_string,
      "DEBUG", cpkP, cpInteger, &_debug,
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
    val = AddressInfo::query_ethernet(device_name + ":eth", en, this);
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
  Vector<String> wifi_cfg;

  if (_wireless_device_config_string != "") {
    cp_spacevec(_wireless_device_config_string, wifi_cfg);

    for( int i = 0; i < wifi_cfg.size(); i++) {

      Element* s_element = router()->find(wifi_cfg[i]);
      if ( s_element != NULL ) {
        if ( s_element->cast("BrnAvailableRates") != NULL ) {
          _wireless_availablerates = (BrnAvailableRates*)s_element->cast("BrnAvailableRates");
        } else if ( s_element->cast("AvailableChannels") != NULL ) {
          _wireless_availablechannels = (AvailableChannels*)s_element->cast("AvailableChannels");
        } else {
          BRN_WARN("Unknown wifi_config: %s", wifi_cfg[i].c_str());
        }
      }
    }
  }

  if ( device_type == DEVICETYPE_WIRELESS ) {
#if CLICK_NS
    _wificonfig = new WifiConfigSim(device_name,router());
#else
#if CLICK_USERLEVEL
    _wificonfig = new WifiConfigIwLib(device_name);
#endif
#endif

    _power = _wificonfig->get_txpower();

    if ( _wireless_availablerates ) {
      _wireless_availablerates->set_max_txpower(_wificonfig->get_max_txpower());

      Vector<MCS> rates;
      _wificonfig->get_rates(rates);

      for(int r = 0; r < rates.size(); r++) {
        BRN_INFO("Rate: %d",rates[r]._rate);
      }

      _wireless_availablerates->set_default_rates(rates);
    }

    BRN_INFO("Channel: %d",_wificonfig->get_channel());
    BRN_INFO("Set Channel(1): %d",_wificonfig->set_channel(1));
    BRN_INFO("Get Channel: %d",_wificonfig->get_channel());

    BRN_INFO("Power: %d Max. Power: %d", _power, _wificonfig->get_max_txpower());

    get_cca();

    get_backoff();
  }

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
  _wificonfig->get_cca(&_cs_threshold, &_rx_threshold, &_cp_threshold);
}

void
BRN2Device::set_cca(int cs_threshold, int rx_threshold, int cp_threshold)
{

  if ( cs_threshold != 0 ) _cs_threshold = cs_threshold;
  if ( rx_threshold != 0 ) _rx_threshold = rx_threshold;
  if ( cp_threshold != 0 ) _cp_threshold = cp_threshold;

  _wificonfig->set_cca(_cs_threshold, _rx_threshold, _cp_threshold);
}

uint32_t
BRN2Device::set_backoff()
{
  _wificonfig->set_backoff(_queue_info);

  return 0;
}

uint32_t
BRN2Device::get_backoff()
{
  _wificonfig->get_backoff(&_queue_info);

  _no_queues = _queue_info[0];
  _cwmin = &_queue_info[2];
  _cwmax = &_queue_info[2+_no_queues];
  _aifs = &_queue_info[2 + (2 * _no_queues)];

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
BRN2Device::set_power(int power, ErrorHandler*)
{
  _wificonfig->set_txpower(power);
  _power = _wificonfig->get_txpower();

  return 0;
}

int
BRN2Device::set_channel(int channel, ErrorHandler*)
{
  _wificonfig->set_channel(channel);
  BRN_DEBUG("Set Channel to: %d", _wificonfig->get_channel());

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

/*****************************************************************************/
/***************** D E V I C E   C O N F I G *********************************/
/*****************************************************************************/
/*void
BRN2Device::parse_queues(String s_cwmin, String s_cwmax, String s_aifs)
{
  uint32_t v;
  Vector<String> args;

  cp_spacevec(s_cwmin, args);
  no_queues = args.size();

  if ( no_queues > 0 ) {
    _cwmin = new uint16_t[no_queues];
    _cwmax = new uint16_t[no_queues];
    _aifs  = new uint16_t[no_queues];

    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmin[i] = v;
      if ( v > _learning_max_bo ) _learning_max_bo = v;
    }
    args.clear();

    cp_spacevec(s_cwmax, args);
    if ( args.size() < no_queues )
      no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _cwmax[i] = v;
    }
    args.clear();

    cp_spacevec(s_aifs, args);
    if ( args.size() < no_queues )
      no_queues = args.size();
    for( int i = 0; i < no_queues; i++ ) {
      cp_integer(args[i], &v);
      _aifs[i] = v;
    }
    args.clear();
  }
}
*/


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_device_info(Element *e, void *)
{
  BRN2Device *dev = (BRN2Device *)e;
  return dev->device_info();
}

static int
write_reset_address(const String &/*in_s*/, Element *e, void *, ErrorHandler */*errh*/)
{
  BRN2Device *dev = (BRN2Device *)e;
  dev->setEtherAddress(dev->getEtherAddressFix());
  return 0;
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
write_power(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2Device *f = (BRN2Device *)e;
  String s = cp_uncomment(in_s);
  uint32_t power;

  if (!cp_integer(s, &power))
   return errh->error("power parameter must be integer");

  f->set_power(power, errh);

  return 0;
}

static int
write_channel(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2Device *f = (BRN2Device *)e;
  String s = cp_uncomment(in_s);
  uint32_t channel;

  if (!cp_integer(s, &channel))
   return errh->error("channel parameter must be integer");

  f->set_channel(channel, errh);

  return 0;
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

static int
write_backoff(const String &in_s, Element *e, void */*vparam*/, ErrorHandler *errh)
{
  BRN2Device *f = (BRN2Device *)e;
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  if (f->get_no_queues() == 0) f->get_backoff();

  if ((f->get_no_queues() * 3) != args.size()) {
    return errh->error("No queues and #params (backoffs) doesn't fit.");
  }

  uint32_t *cwmin = f->get_cwmin();
  uint32_t *cwmax = f->get_cwmax();
  uint32_t *aifs = f->get_aifs();
  int i = 0;
  for ( int q = 0; q < f->get_no_queues(); q++ ) {
    if (!cp_integer(args[i++], &(cwmin[q]))) return errh->error("cwmin parameter must be integer");
    if (!cp_integer(args[i++], &(cwmax[q]))) return errh->error("cwmax parameter must be integer");
    if (!cp_integer(args[i++], &(aifs[q]))) return errh->error("aifs parameter must be integer");
  }

  f->set_backoff();
  //f->get_backoff(); //check, whether backoff is set in simulator

  return 0;
}
void
BRN2Device::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("deviceinfo", read_device_info, 0);

  add_write_handler("reset_address", write_reset_address, 0);
  add_write_handler("address", write_address, 0);
  add_write_handler("power", write_power, 0);
  add_write_handler("channel", write_channel, 0);
  add_write_handler("cca", write_cca, 0);
  add_write_handler("backoff", write_backoff, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Device)
