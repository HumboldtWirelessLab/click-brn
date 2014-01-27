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

void *
RtsCtsPacketSize::cast(const char *name)
{
  if (strcmp(name, "RtsCtsPacketSize") == 0)
    return (RtsCtsPacketSize *) this;
  else if (strcmp(name, "RtsCtsScheme") == 0)
    return (RtsCtsScheme *) this;
  else
    return NULL;
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
  //BRN_WARN("Size: %d",size);
  return (size >= _psize);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsPacketSize)

