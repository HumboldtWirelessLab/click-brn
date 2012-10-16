#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "elements/wifi/athdesc.h"
#include "ath2encap.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/wifi/brnwifi.hh"

CLICK_DECLS


Ath2Encap::Ath2Encap()
{
}

Ath2Encap::~Ath2Encap()
{
}

int
Ath2Encap::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _athencap = true;

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
      "ATHENCAP", cpkN, cpBool, &_athencap,
      "DEBUG", 0, cpBool, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

Packet *
Ath2Encap::simple_action(Packet *p)
{
  WritablePacket *p_out;
  struct ath2_header *ath2_h;
  uint8_t channel;

  if ( _athencap )
    p_out = p->push(ATHDESC2_HEADER_SIZE);       //ATH-HEADER and ATH_BRN-HEADER
  else
    p_out = p->push(ATHDESC2_BRN_HEADER_SIZE);   //ATH_BRN-HEADER only

  if (!p_out) { return 0; }

  click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_out);

  if ( _athencap ) {
    struct ar5212_desc *desc  = (struct ar5212_desc *) (p_out->data() + 8);

    memset((void *)p_out->data(), 0, ATHDESC2_HEADER_SIZE);  //set all to zero ( ath and ath_brn )
    ath2_h = (struct ath2_header*)(p_out->data() + ATHDESC_HEADER_SIZE);

    desc->xmit_power = ceh->power;
    desc->xmit_rate0 = dot11_to_ratecode(ceh->rate);
    desc->xmit_rate1 = dot11_to_ratecode(ceh->rate1);
    desc->xmit_rate2 = dot11_to_ratecode(ceh->rate2);
    desc->xmit_rate3 = dot11_to_ratecode(ceh->rate3);

    if ( ( ceh->flags & WIFI_EXTRA_DO_RTS_CTS ) != 0 ) desc->rts_cts_enable = 1;
    if ( ( ceh->flags & WIFI_EXTRA_DO_CTS ) != 0 ) desc->cts_enable = 1;
    if ( ( ceh->flags & WIFI_EXTRA_RX_MORE ) != 0 ) desc->more = 1;

    if (ceh->max_tries > 0) desc->xmit_tries0 = ceh->max_tries;
    if (ceh->max_tries1 > 0) desc->xmit_tries1 = ceh->max_tries1;
    if (ceh->max_tries2 > 0) desc->xmit_tries2 = ceh->max_tries2;
    if (ceh->max_tries3 > 0) desc->xmit_tries3 = ceh->max_tries3;
  }
  else
  {
    memset((void *)p_out->data(), 0, ATHDESC2_BRN_HEADER_SIZE);
    ath2_h = (struct ath2_header*)(p_out->data());
  }

  channel = BRNPacketAnno::channel_anno(p);
  ath2_h->anno.tx_anno.channel = channel;

  ath2_h->anno.tx_anno.queue = BrnWifi::getTxQueue(ceh);

  ath2_h->ath2_version = htons(ATHDESC2_VERSION);
  ath2_h->madwifi_version = htons(MADWIFI_TRUNK);

  return p_out;
}


enum {H_DEBUG};

static String 
Ath2Encap_read_param(Element *e, void *thunk)
{
  Ath2Encap *td = (Ath2Encap *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      default:
        return String();
    }
}

static int 
Ath2Encap_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  Ath2Encap *f = (Ath2Encap *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
Ath2Encap::add_handlers()
{
  add_read_handler("debug", Ath2Encap_read_param, (void *) H_DEBUG);
  add_write_handler("debug", Ath2Encap_write_param, (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Encap)
