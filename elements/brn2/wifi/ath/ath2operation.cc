#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "elements/wifi/athdesc.h"
#include "ath2operation.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

CLICK_DECLS


Ath2Operation::Ath2Operation()
   : _timer(this)
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
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
Ath2Operation::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(100);

  return 0;
}

void
Ath2Operation::run_timer(Timer *t)
{
  read_config();
}

void
Ath2Operation::push(int /*port*/,Packet *p)
{
  BRN_DEBUG("Receive result of operation");

  BRN_DEBUG("Size is %d", p->length());
  struct ath2_header *ath2_h = (struct ath2_header*)(p->data() + ATHDESC_HEADER_SIZE);

  if ( (ntohs(ath2_h->ath2_version) == ATHDESC2_VERSION) && (ntohs(ath2_h->madwifi_version) == MADWIFI_TRUNK) ) {
    BRN_DEBUG("ATH version is correct");

    if ( (ath2_h->flags & MADWIFI_FLAGS_GET_CONFIG) != 0 ) {
      BRN_DEBUG("Get read config result");

      channel = ath2_h->anno.rx_anno.channel;                   //channel to set
      cu_pkt_threshold = ath2_h->anno.rx_anno.cu_pkt_threshold; //channel utility: rx time
      cu_update_mode = ath2_h->anno.rx_anno.cu_update_mode;     //channel utility: tx time
      cu_anno_mode = ath2_h->anno.rx_anno.cu_anno_mode;         //channel utility: rx time

      memcpy(cw_min, ath2_h->anno.rx_anno.cw_min,4);
      memcpy(cw_max, ath2_h->anno.rx_anno.cw_max,4);
      memcpy(aifs, ath2_h->anno.rx_anno.aifs,4);

      cca_threshold = ath2_h->anno.rx_anno.cca_threshold;

      driver_flags = ath2_h->flags;
    }
  }

  p->kill();
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

String
Ath2Operation::madwifi_config()
{
  StringAccum sa;

  sa << "Madwifi config:\n";
  sa << "Channel: " << (int)channel << "\n";
  sa << "cu treshold: " << (int)cu_pkt_threshold << "\n";
  sa << "cu update: " << (int)cu_update_mode << "\n";
  sa << "cu anno mode: " << (int)cu_anno_mode << "\n";

  for ( int i = 0; i < 4; i++ ) {
    sa << "Queue " << i << ": " << (int)cw_min[i] << " " << (int)cw_max[i] << " " << (int)aifs[i] << "\n";
  }

  sa << "CCA: " << (int)cca_threshold << "\n";
  sa << "Flags: " << driver_flags << "\n";

  return sa.take_string();
}

enum {H_CHANNEL, H_CONFIG};

static String 
Ath2Operation_read_param(Element *e, void *thunk)
{
  Ath2Operation *td = (Ath2Operation *)e;

  switch ((uintptr_t) thunk) {
    case H_CONFIG: {
      return td->madwifi_config();
      break;
    }
    default:
      return String();
  }
}

static int 
Ath2Operation_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Ath2Operation *f = (Ath2Operation *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_CHANNEL: {    //channel
        int channel;
        if (!cp_integer(s, &channel))
          return errh->error("channel parameter must be integer");
        f->set_channel(channel);
        break;
      }
    case H_CONFIG: {
        f->read_config();
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
  add_write_handler("config", Ath2Operation_write_param, (void *) H_CONFIG);

  add_read_handler("config", Ath2Operation_read_param, (void *) H_CONFIG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Operation)
