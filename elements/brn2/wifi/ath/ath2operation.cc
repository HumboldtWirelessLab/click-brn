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
{
}

Ath2Operation::~Ath2Operation()
{
}

int
Ath2Operation::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _debug = false;
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", 0, cpBool, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

void
Ath2Operation::push(int port,Packet *p)
{
  WritablePacket *p_out;
  struct ath2_header *ath2_h;
  uint8_t channel;

  p->kill();
}


void
Ath2Operation::set_channel(int channel)
{
  struct ath2_header *ath2_h;

  WritablePacket *new_packet = WritablePacket::make(ATHDESC2_HEADER_SIZE);
  memset((void *)new_packet->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )

  ath2_h = (struct ath2_header*)(new_packet->data() + ATHDESC_HEADER_SIZE);

  ath2_h->ath2_version = ATHDESC2_VERSION;
  ath2_h->madwifi_version = MADWIFI_TRUNK;

  ath2_h->flags |= ATH2_FLAGS_IS_OPERATION;

  ath2_h->anno.tx_anno.operation |= ATH2_OPERATION_SETCHANNEL;
  ath2_h->anno.tx_anno.channel = channel;

  output(0).push(new_packet);
}

enum {H_DEBUG,
      H_CHANNEL};

static String 
Ath2Operation_read_param(Element *e, void *thunk)
{
  Ath2Operation *td = (Ath2Operation *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
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
    case H_DEBUG: {    //debug
        bool debug;
        if (!cp_bool(s, &debug))
          return errh->error("debug parameter must be boolean");
        f->_debug = debug;
        break;
      }
    case H_CHANNEL: {    //channel
        int channel;
        if (!cp_integer(s, &channel))
          return errh->error("channel parameter must be integer");
        f->set_channel(channel);
        break;
      }
  }
  return 0;
}

void
Ath2Operation::add_handlers()
{
  add_read_handler("debug", Ath2Operation_read_param, (void *) H_DEBUG);
  add_write_handler("debug", Ath2Operation_write_param, (void *) H_DEBUG);
  add_write_handler("channel", Ath2Operation_write_param, (void *) H_CHANNEL);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Operation)
