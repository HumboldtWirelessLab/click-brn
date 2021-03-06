#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include <click/userutils.hh>
#include <unistd.h>

#include "elements/wifi/athdesc.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "ath2operation.hh"

CLICK_DECLS


Ath2Operation::Ath2Operation()
   : _device(NULL),
     _timer(this),
     _read_config(true),
     _valid_driver_flags(false)
{
  BRNElement::init();
  channel = 0;
  cu_pkt_threshold = 0;
  cu_update_mode = 0;
  cu_anno_mode = 0;

  memset(cw_min, 0,4);
  memset(cw_max, 0,4);
  memset(aifs, 0,4);

  cca_threshold = 0;

  driver_flags = 0;
}

Ath2Operation::~Ath2Operation()
{
}

int
Ath2Operation::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEVICE", cpkP, cpElement, &_device,
      "READCONFIG", cpkP, cpBool, &_read_config,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
Ath2Operation::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  if ( _read_config ) {
    _timer.schedule_after_msec(100);
  }

  return 0;
}

void
Ath2Operation::run_timer(Timer */*t*/)
{
  read_config();
}

void
Ath2Operation::push(int /*port*/,Packet *p)
{
  struct ath2_header *ath2_h = (struct ath2_header*)(p->data() + ATHDESC_HEADER_SIZE);

  if ( (ntohs(ath2_h->ath2_version) == ATHDESC2_VERSION) && (ntohs(ath2_h->madwifi_version) == MADWIFI_TRUNK) ) {
    BRN_DEBUG("Receive result of operation. Size: %d", p->length());
    BRN_DEBUG("ATH version is correct");

    if ( (ath2_h->flags & MADWIFI_FLAGS_GET_CONFIG) != 0 ) {
      BRN_DEBUG("Get read config result");

      channel = ath2_h->anno.rx_anno.channel;                   //channel to set
      if ( _device ) _device->setChannel(channel);

      cu_pkt_threshold = ath2_h->anno.rx_anno.cu_pkt_threshold; //channel utility: rx time
      cu_update_mode = ath2_h->anno.rx_anno.cu_update_mode;     //channel utility: tx time
      cu_anno_mode = ath2_h->anno.rx_anno.cu_anno_mode;         //channel utility: rx time

      memcpy(cw_min, ath2_h->anno.rx_anno.cw_min,4);
      memcpy(cw_max, ath2_h->anno.rx_anno.cw_max,4);
      memcpy(aifs, ath2_h->anno.rx_anno.aifs,4);

      cca_threshold = ath2_h->anno.rx_anno.cca_threshold;

      driver_flags = (ath2_h->flags & 0x0FFFFFFF);

      _valid_driver_flags = true;
    }

    if ( (ath2_h->flags & MADWIFI_FLAGS_SET_CONFIG) != 0 ) {
      driver_flags = (ath2_h->flags & 0x0FFFFFFF);
      _valid_driver_flags = true;
    }

    if ( (ath2_h->flags & MADWIFI_FLAGS_IS_OPERATION) != 0 ) {
      //handle operation
    }
  }

  p->kill();
}

inline
WritablePacket *new_ath_operation_packet()
{
  WritablePacket *new_packet = WritablePacket::make(ATHDESC2_HEADER_SIZE);
  memset((void *)new_packet->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )

  struct ath2_header *ath2_h = (struct ath2_header*)(new_packet->data() + ATHDESC_HEADER_SIZE);

  ath2_h->ath2_version = htons(ATHDESC2_VERSION);
  ath2_h->madwifi_version = htons(MADWIFI_TRUNK);

  ath2_h->flags |= MADWIFI_FLAGS_IS_OPERATION;

  return new_packet;
}

void
Ath2Operation::set_channel(int channel)
{
  struct ath2_header *ath2_h;

  WritablePacket *new_packet = WritablePacket::make(ATHDESC2_HEADER_SIZE);
  memset((void *)new_packet->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )

  BRN_DEBUG("Set channel %d",channel);
  ath2_h = (struct ath2_header*)(new_packet->data() + ATHDESC_HEADER_SIZE);

  ath2_h->ath2_version = htons(ATHDESC2_VERSION);
  ath2_h->madwifi_version = htons(MADWIFI_TRUNK);

  ath2_h->flags |= MADWIFI_FLAGS_IS_OPERATION;

  ath2_h->anno.tx_anno.operation |= ATH2_OPERATION_SET_CHANNEL;
  ath2_h->anno.tx_anno.channel = channel;

  if ( _device ) _device->setChannel(channel);  //TODO: move to push. set channel only if operation was
                                                //      successful

  output(0).push(new_packet);
}

void
Ath2Operation::set_mac(EtherAddress *mac)
{
  struct ath2_header *ath2_h;

  WritablePacket *new_packet = WritablePacket::make(ATHDESC2_HEADER_SIZE);
  memset((void *)new_packet->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )

  BRN_DEBUG("Set mac %s",mac->unparse().c_str());
  ath2_h = (struct ath2_header*)(new_packet->data() + ATHDESC_HEADER_SIZE);

  ath2_h->ath2_version = htons(ATHDESC2_VERSION);
  ath2_h->madwifi_version = htons(MADWIFI_TRUNK);

  ath2_h->flags |= MADWIFI_FLAGS_IS_OPERATION;

  ath2_h->anno.tx_anno.operation |= ATH2_OPERATION_SET_MAC;
  memcpy(ath2_h->anno.tx_anno.mac, mac->data(), 6);

  if ( _device ) _device->setEtherAddress(mac);  //TODO: move to push. set mac only if operation was
                                                 //      successful

  output(0).push(new_packet);
}

void
Ath2Operation::read_config()
{
  struct ath2_header *ath2_h;

  WritablePacket *new_packet = WritablePacket::make(ATHDESC2_HEADER_SIZE);
  memset((void *)new_packet->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )

  BRN_DEBUG("Read config");
  ath2_h = (struct ath2_header*)(new_packet->data() + ATHDESC_HEADER_SIZE);

  ath2_h->ath2_version = htons(ATHDESC2_VERSION);
  ath2_h->madwifi_version = htons(MADWIFI_TRUNK);

  ath2_h->flags |= MADWIFI_FLAGS_GET_CONFIG;

  output(0).push(new_packet);
}

void
Ath2Operation::set_macclone(bool macclone)
{
  struct ath2_header *ath2_h;

  if ( ! _valid_driver_flags ) {
    BRN_WARN("Read config before change it !");
    return;
  }

  WritablePacket *new_packet = WritablePacket::make(ATHDESC2_HEADER_SIZE);
  memset((void *)new_packet->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )

  ath2_h = (struct ath2_header*)(new_packet->data() + ATHDESC_HEADER_SIZE);

  ath2_h->ath2_version = htons(ATHDESC2_VERSION);
  ath2_h->madwifi_version = htons(MADWIFI_TRUNK);

  ath2_h->flags = driver_flags | MADWIFI_FLAGS_SET_CONFIG;

  if ( macclone ) ath2_h->flags |= MADWIFI_FLAGS_MACCLONE_ENABLED;
  else ath2_h->flags &= ~MADWIFI_FLAGS_MACCLONE_ENABLED;

  output(0).push(new_packet);
}

/**************************************************************/
/********************** H A N D L E R *************************/
/**************************************************************/

String
Ath2Operation::madwifi_config()
{
  StringAccum sa;

  sa << "Wifidevice_config:\n";
  if ( _device ) sa << "Channel: " << (int)_device->getChannel() << "\n";
  else           sa << "Channel: " << (int)channel << "\n";
  sa << "cu treshold: " << (int)cu_pkt_threshold << "\n";
  sa << "cu update: " << (int)cu_update_mode << "\n";
  sa << "cu anno mode: " << (int)cu_anno_mode << "\n";

  for ( int i = 0; i < 4; i++ ) {
    sa << "Queue " << i << ": " << (int)cw_min[i] << " " << (int)cw_max[i] << " " << (int)aifs[i] << "\n";
  }

  sa << "CCA treshold: " << (int)cca_threshold << "\n";

  sa << "Flags:\n";
  sa << " CCA: " << (((driver_flags & MADWIFI_FLAGS_CCA_ENABLED) != 0)  ? "1" : "0") << "\n";
  sa << " Small Backoff: " << (((driver_flags & MADWIFI_FLAGS_SMALLBACKOFF_ENABLED) != 0) ? "1" : "0") << "\n";
  sa << " Burst: " << (((driver_flags & MADWIFI_FLAGS_BURST_ENABLED) != 0) ? "1" : "0") << "\n";
  sa << " Channel Switch: " << (((driver_flags & MADWIFI_FLAGS_CHANNELSWITCH_ENABLED) != 0) ? "1" : "0") << "\n";
  sa << " Macclone: " << (((driver_flags & MADWIFI_FLAGS_MACCLONE_ENABLED) != 0) ? "1" : "0") << "\n";

  return sa.take_string();
}

String
Ath2Operation::read_packet_count()
{
  String raw_info = file_string("/proc/net/madwifi/" + _device->getDeviceName() + "/packet_stats");

  Vector<String> info;
  cp_spacevec(raw_info, info);

  return String(info[1] + " " + info[3] + " " + info[5] + " " + info[7] + " " + info[9]);
}

void
Ath2Operation::reset_packet_count()
{
  return;
}

void
Ath2Operation::clear_hw_queues(String devname, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
  StringAccum cmda;
  if (access("/sbin/sysctl", X_OK) == 0)
    cmda << "/sbin/sysctl";
  else if (access("/usr/sbin/sysctl", X_OK) == 0)
    cmda << "/usr/sbin/sysctl";
  else
    return;

  cmda << " -w dev." << devname << ".cleartxqueue=0 > /dev/null";
  String cmd = cmda.take_string();

  BRN_DEBUG("Clear hw queue: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", errh);
  if (out)
    BRN_ERROR("%s: %s", cmd.c_str(), out.c_str());
#endif

  return;
}

enum {H_CHANNEL, H_RESET_MAC, H_MAC, H_CONFIG, H_CCA_THRESHOLD, H_PKT_COUNT, H_CLEAR_HW_QUEUES, H_SET_MACCLONE};

static String 
Ath2Operation_read_param(Element *e, void *thunk)
{
  Ath2Operation *athop = reinterpret_cast<Ath2Operation *>(e);

  switch ((uintptr_t) thunk) {
    case H_CONFIG:    return athop->madwifi_config();
    case H_PKT_COUNT: return athop->read_packet_count();
    default:          return String();
  }
}

static int 
Ath2Operation_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Ath2Operation *athop = reinterpret_cast<Ath2Operation *>(e);
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_CHANNEL: {       //channel
        int channel;
        if (!cp_integer(s, &channel))
          return errh->error("channel parameter must be integer");
        athop->set_channel(channel);
        break;
      }
    case H_MAC: {           //mac
        EtherAddress mac;
        if (!cp_ethernet_address(s, &mac))
          return errh->error("mac parameter must be etheraddress");
        athop->set_mac(&mac);
        break;
      }
   case H_RESET_MAC: {     //reset mac
        if (athop->_device) {
          athop->_device->setEtherAddress(athop->_device->getEtherAddressFix());
          athop->set_mac(athop->_device->getEtherAddressFix());
        }
        break;
      }
   case H_CCA_THRESHOLD: { //cca threshold
        int cca_threshold;
        if (!cp_integer(s, &cca_threshold))
          return errh->error("cca_threshold parameter must be integer");
        //athop->set_cca_threshold(&cca_threshold);
        break;
      }
    case H_CONFIG: {
        athop->read_config();
        break;
      }
    case H_PKT_COUNT: {
        athop->reset_packet_count();
        break;
      }
    case H_CLEAR_HW_QUEUES: {
        String devicename;
        if (!cp_string(s, &devicename))
          return errh->error("devname parameter must be string");
        athop->clear_hw_queues(devicename, errh);
        break;
      }
    case H_SET_MACCLONE: {       //mac_clone
        bool macclone;
        if (!cp_bool(s, &macclone))
          return errh->error("macclone parameter must be bool");
        athop->set_macclone(macclone);
        break;
      }
  }
  return 0;
}

void
Ath2Operation::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("channel", Ath2Operation_write_param, (void *) H_CHANNEL);
  add_write_handler("mac", Ath2Operation_write_param, (void *) H_MAC);
  add_write_handler("reset_mac", Ath2Operation_write_param, (void *) H_RESET_MAC);
  add_write_handler("cca_threshold", Ath2Operation_write_param, (void *) H_CCA_THRESHOLD);
  add_write_handler("read_config", Ath2Operation_write_param, (void *) H_CONFIG);
  add_write_handler("reset_pkt_count", Ath2Operation_write_param, (void *) H_PKT_COUNT);
  add_write_handler("clear_hw_queues", Ath2Operation_write_param, (void *) H_CLEAR_HW_QUEUES);
  add_write_handler("set_macclone", Ath2Operation_write_param, (void *) H_SET_MACCLONE);

  add_read_handler("config", Ath2Operation_read_param, (void *) H_CONFIG);
  add_read_handler("pkt_count", Ath2Operation_read_param, (void *) H_PKT_COUNT);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(Ath2Operation)
