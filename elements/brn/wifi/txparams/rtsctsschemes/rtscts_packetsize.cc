#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include <elements/wifi/bitrate.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include "rtscts_packetsize.hh"

CLICK_DECLS

RtsCtsPacketSize::RtsCtsPacketSize():
  _psize(0),
  _pduration(0)
{
  _default_strategy = RTS_CTS_STRATEGY_SIZE_LIMIT;
}

void *
RtsCtsPacketSize::cast(const char *name)
{
  if (strcmp(name, "RtsCtsPacketSize") == 0)
    return dynamic_cast<RtsCtsPacketSize *>(this);

  return RtsCtsScheme::cast(name);
}

RtsCtsPacketSize::~RtsCtsPacketSize()
{
}

int
RtsCtsPacketSize::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "PACKETSIZE", cpkP, cpInteger, &_psize,
    "PACKETDURATION", cpkP, cpInteger, &_pduration,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;

  if ( _pduration == -1 ) {
    _pduration = calc_transmit_time(2, 16/*RTS*/ + 10 /*CTS*/);
  }

  return 0;
}

bool
RtsCtsPacketSize::set_rtscts(PacketInfo *pinfo)
{
  if ( _pduration != 0 ) {
    //if ( pinfo->_ceh _is_ht )
    //  duration = BrnWifi::pkt_duration(pinfo->_p_size + 4 /*crc*/, rate_index, rate_bw, rate_sgi);
    //else
    int32_t duration = calc_transmit_time(pinfo->_ceh->rate, pinfo->_p_size + 4 /*crc*/); //TODO: check CRC ??
    BRN_WARN("Duration limit: %d cdur: %d", _pduration, duration);
    return ( duration > _pduration );
  }

  return (_psize <= pinfo->_p_size);
}

enum {H_THRESHOLD_PACKETSIZE, H_THRESHOLD_PACKETDURATION};

static String RtsCtsPacketSize_read_param(Element *e, void *thunk)
{
  RtsCtsPacketSize *f = reinterpret_cast<RtsCtsPacketSize *>(e);
  switch ((uintptr_t) thunk) {
    case H_THRESHOLD_PACKETSIZE:
      return String(f->_psize);
    case H_THRESHOLD_PACKETDURATION:
      return String(f->_pduration);
    default:
      return String();
  }
}


static int RtsCtsPacketSize_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  RtsCtsPacketSize *f = reinterpret_cast<RtsCtsPacketSize *>(e);
  String s = cp_uncomment(in_s);
  int value;
  if (!cp_integer(s, &value)) return errh->error("rtscts_packetsize parameter must be integer");

  switch((intptr_t)vparam) {
    case H_THRESHOLD_PACKETSIZE:
      f->_psize = value;
      break;
    case H_THRESHOLD_PACKETDURATION:
      f->_pduration = value;
      break;
  }
  return 0;
}


void
RtsCtsPacketSize::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_write_handler("packetsize", RtsCtsPacketSize_write_param, H_THRESHOLD_PACKETSIZE);
  add_write_handler("packetduration", RtsCtsPacketSize_write_param, H_THRESHOLD_PACKETDURATION);

  add_read_handler("packetsize", RtsCtsPacketSize_read_param, H_THRESHOLD_PACKETSIZE);
  add_read_handler("packetduration", RtsCtsPacketSize_read_param, H_THRESHOLD_PACKETDURATION);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsPacketSize)

