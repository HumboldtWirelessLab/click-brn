#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "rtscts_random.hh"

CLICK_DECLS

RtsCtsRandom::RtsCtsRandom()
{
}

void *
RtsCtsRandom::cast(const char *name)
{
  if (strcmp(name, "RtsCtsRandom") == 0)
    return (RtsCtsRandom *) this;
  else if (strcmp(name, "RtsCtsScheme") == 0)
    return (RtsCtsScheme *) this;
  else
    return NULL;
}

RtsCtsRandom::~RtsCtsRandom()
{
}

int
RtsCtsRandom::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "PROBABILITY", cpkP, cpInteger, &_prob,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

bool
RtsCtsRandom::set_rtscts(EtherAddress &/*dst*/, uint32_t /*size*/)
{
  if (_prob == 0) return false;

  return (click_random(0, 100) <= _prob)?true:false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RtsCtsRandom)

