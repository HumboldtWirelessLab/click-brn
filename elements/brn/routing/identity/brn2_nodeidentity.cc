#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/standard/addressinfo.hh>

#if CLICK_NS
#include <click/router.hh>
#endif

#include <elements/brn/brn2.h>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "brn2_nodeidentity.hh"

CLICK_DECLS

BRN2NodeIdentity::BRN2NodeIdentity()
  : _node_devices(NULL),
    _node_devices_size(0),
    _node_id_32(0),
    _master_device(NULL),
    _master_device_id(-1),
    _service_device(NULL),
    _service_device_id(-1),
    _instance_id("n/a"),
    _instance_owner("n/a"),
    _instance_group("n/a")
{
  BRNElement::init();
  memset(_node_id,0,sizeof(_node_id));
  memset(_click_binary_id,0,sizeof(_click_binary_id));
  memset(_click_script_id,0,sizeof(_click_script_id));
}

BRN2NodeIdentity::~BRN2NodeIdentity()
{
  delete[] _node_devices;
}

int
BRN2NodeIdentity::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int no_dev = 0;
  String dev_string;
  Vector<String> devices;

  _nodename="auto";

  if (cp_va_kparse(conf, this, errh,
      "DEVICES", cpkP+cpkM, cpString, &dev_string,
#if CLICK_NS
      "NAME", cpkP, cpString, &_nodename,
#else
      "NAME", cpkP+cpkM, cpString, &_nodename,
#endif
      cpEnd) < 0)
    return -1;

  String dev_string_uncomment = cp_uncomment(dev_string);
  cp_spacevec(dev_string_uncomment, devices);

  _node_devices_size = 0;
  _node_devices = new BRN2Device*[devices.size()];

  for (int slot = 0; slot < devices.size(); slot++) {
    Element *e = cp_element(devices[slot], this, errh);
    BRN2Device *brn_device = reinterpret_cast<BRN2Device *>(e->cast("BRN2Device"));
    if (!brn_device) {
      return errh->error("element is not an BRN2Device");
    } else {
      brn_device->setDeviceNumber(no_dev++);
      _node_devices[_node_devices_size] = brn_device;
      _node_devices_size++;
    }
  }

  if ( _node_devices_size > 0 ) {
    _master_device = _node_devices[0];
  }

  return 0;
}

int
BRN2NodeIdentity::initialize(ErrorHandler *)
{
  BRN2Device *brn_device;

  for ( uint32_t i = 0; i < _node_devices_size; i++ ) {
    brn_device = _node_devices[i];

    if ( brn_device->is_master_device() ) {
      _master_device = brn_device;
      _master_device_id = brn_device->getDeviceNumber();
   }

    if ( brn_device->is_service_device() ) {
      _service_device = brn_device;
      _service_device_id = brn_device->getDeviceNumber();
    }
  }

  if ( _master_device_id == -1 ) {
    _master_device = _node_devices[0];
    _master_device_id = 0;
    //!! First set the device, than print debug !!//
    BRN_DEBUG("No master device: use 0 for master");
  }

  if ( _service_device_id == -1 ) {
    _service_device = _node_devices[0];
    _service_device_id = 0;
    //!! First set the device, than print debug !!//
    BRN_DEBUG("No service device: use 0 for service");
  }

  if ( (_nodename == "") || (_nodename == "auto") ) {
#if CLICK_NS
#define NAME_BUFFER_LEN 256
    char buf[NAME_BUFFER_LEN];
    simclick_sim_command(router()->simnode(), SIMCLICK_GET_NODE_NAME, &buf, NAME_BUFFER_LEN );
    _nodename = String(buf);
#else
    assert(_master_device != NULL);
    _nodename = _master_device->getEtherAddress()->unparse();
#endif
  }

  assert(_master_device != NULL);
  BRN_INFO("MasterDevice: %s",_master_device->getEtherAddress()->unparse().c_str());

  assert(_master_device != NULL);
  MD5::calculate_md5((const char*)MD5::convert_ether2hex(_master_device->getEtherAddress()->data()).c_str(),
                    strlen((const char*)MD5::convert_ether2hex(_master_device->getEtherAddress()->data()).c_str()), _node_id );

  _node_id_32 = ((uint8_t*)_node_id)[0];
  _node_id_32 = _node_id_32 * 256 + ((uint8_t*)_node_id)[1];
  _node_id_32 = _node_id_32 * 256 + ((uint8_t*)_node_id)[2];
  _node_id_32 = _node_id_32 * 256 + ((uint8_t*)_node_id)[3];

  return 0;
}

/* returns true if the given ethernet address belongs to this node (e.g. wlan-dev)*/
bool
BRN2NodeIdentity::isIdentical(const EtherAddress *e)
{
  for ( uint32_t i = 0; i < _node_devices_size; i++ )
    if ( memcmp(e->data(), _node_devices[i]->getEtherAddress()->data(),6) == 0 ) return true;

  return false;
}

bool
BRN2NodeIdentity::isIdentical(const uint8_t *data)
{
  for ( uint32_t i = 0; i < _node_devices_size; i++ )
    if ( memcmp(data, _node_devices[i]->getEtherAddress()->data(),6) == 0 ) return true;

  return false;
}

int
BRN2NodeIdentity::getDeviceNumber(EtherAddress *e) {
  for ( uint32_t i = 0; i < _node_devices_size; i++ )
    if ( memcmp(e->data(), _node_devices[i]->getEtherAddress()->data(),6) == 0 ) return _node_devices[i]->getDeviceNumber();

  return -1;
}

BRN2Device *
BRN2NodeIdentity::getDeviceByNumber(uint8_t num) {
  for ( uint32_t i = 0; i < _node_devices_size; i++ )
    if ( num == _node_devices[i]->getDeviceNumber() ) return _node_devices[i];

  return NULL;
}

BRN2Device *
BRN2NodeIdentity::getDeviceByIndex(uint8_t index) {
  if ( index < _node_devices_size ) return _node_devices[index];

  return NULL;
}

const EtherAddress *
BRN2NodeIdentity::getMainAddress() {
  assert(_master_device != NULL);
  return _master_device->getEtherAddress();
}

const EtherAddress *
BRN2NodeIdentity::getMasterAddress() {
  assert(_master_device != NULL);
  return _master_device->getEtherAddress();
}

const EtherAddress *
BRN2NodeIdentity::getServiceAddress() {
  return _service_device->getEtherAddress();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_devinfo_param(Element *e, void *)
{
  StringAccum sa;
  BRN2NodeIdentity *id = reinterpret_cast<BRN2NodeIdentity *>(e);

  sa << "<nodeidentity name=\"" << id->_nodename << "\">\n";
  for ( uint32_t i = 0; i < id->_node_devices_size; i++ ) {
    BRN2Device *dev = id->_node_devices[i];
    sa << "\t<device index=\"" << i << "\" name=\"" << dev->getDeviceName().c_str();
    sa << "\" ethernet_address=\"" << dev->getEtherAddress()->unparse().c_str();
    sa << "\" ip_address=\"" << dev->getIPAddress()->unparse().c_str();
    sa << "\" type=\"" << dev->getDeviceTypeString().c_str()  << "\" />\n";
  }
  sa << "</nodeidentity>\n";

  return sa.take_string();
}

static String
read_version_param(Element *e, void *)
{
  StringAccum sa;
  BRN2NodeIdentity *id = reinterpret_cast<BRN2NodeIdentity *>(e);

  char click_binary_digest[16*2 + 1];
  char click_script_digest[16*2 + 1];

  MD5::printDigest(id->_click_binary_id, click_binary_digest);
  MD5::printDigest(id->_click_script_id, click_script_digest);

  sa << "<version name=\"" << id->_nodename << "\">\n";
  sa << "\t<click_binary version=\"" << CLICK_VERSION << "\" brn_version=\"" << BRN_VERSION << "\"";
  sa << " build_date=\"" << __DATE__ << "\" build_time=\"" << __TIME__ << "\"";
#if CLICK_USERLEVEL == 1
#if CLICK_NS == 1
  sa << " mode=\"sim\"";
#else
  sa << " mode=\"userlevel\"";
#endif
#else
#ifdef CLICK_LINUXMODULE
  sa << " mode=\"kernel\"";
#else
  sa << " mode=\"unknown\"";
#endif
#endif
  sa << " md5_id=\"" << click_binary_digest << "\" git_version=\"" << BRN_GIT_VERSION << "\" />\n";
  sa << "\t<click_script md5_id=\"" << click_script_digest << "\" />\n";
  sa << "\t<instance id=\"" << id->_instance_id << "\"  owner=\"" << id->_instance_owner << "\" group=\"";
  sa << id->_instance_group << "\" />\n";
  sa << "</version>\n";

  return sa.take_string();
}

static int
write_nodename_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BRN2NodeIdentity *id = reinterpret_cast<BRN2NodeIdentity *>(e);
  String s = cp_uncomment(in_s);
  String nodename;
  if (!cp_string(s, &nodename))
    return errh->error("nodename parameter string");
  id->_nodename = nodename;
  return 0;
}

static int
write_instance_param(const String &in_s, Element *e, void *, ErrorHandler */*errh*/)
{
  BRN2NodeIdentity *id = reinterpret_cast<BRN2NodeIdentity *>(e);
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  if ( args.size() >= 3 ) {
    id->_instance_id = args[0];
    id->_instance_owner = args[1];
    id->_instance_group = args[2];
  }

  return 0;
}
static int
write_version_param(const String &in_s, Element *e, void *, ErrorHandler */*errh*/)
{
  BRN2NodeIdentity *id = reinterpret_cast<BRN2NodeIdentity *>(e);
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  if ( args.size() == 2) {
    MD5::digestFromString(id->_click_binary_id, args[0].c_str());
    MD5::digestFromString(id->_click_script_id, args[1].c_str());
  }

  return 0;
}


void
BRN2NodeIdentity::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("info", read_devinfo_param, 0);
  add_write_handler("nodename", write_nodename_param, 0);
  add_write_handler("instance", write_instance_param, 0);

  add_read_handler("version", read_version_param, 0);
  add_write_handler("version", write_version_param, 0);
}

#include <click/vector.cc>
template class Vector<BRN2Device*>;

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2NodeIdentity)
