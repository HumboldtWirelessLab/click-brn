#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "rtscts_packetsize.hh"

CLICK_DECLS

RtsCtsPacketSize::RtsCtsPacketSize()
{
}

RtsCtsPacketSize::~RtsCtsPacketSize()
{
}

int
RtsCtsPacketSize::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "PACKETSIZE", cpkP, cpInteger, &_psize,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsPacketSize::set_rtscts(EtherAddress &/*dst*/, uint32_t size)
{
  return (size >= _psize);
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(RtsCtsPacketSize)

