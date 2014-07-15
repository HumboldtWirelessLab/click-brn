#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>

#include "elements/brn/brn2.h"

#include "pullstats.hh"

CLICK_DECLS

PullStats::PullStats():
  _last_pull_null(false), _pull_null_duration(0)
{
  BRNElement::init();
}

PullStats::~PullStats()
{
}

int
PullStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", 0, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}


int
PullStats::initialize(ErrorHandler */*errh*/)
{
  return 0;
}

Packet *
PullStats::pull(int /*port*/)
{
  if (Packet *p = input(0).pull()) {
    //fprintf(stderr,"Hey!!! Sending a packet!!!\n");
    if (_last_pull_null) {
      _pull_null_duration += (Timestamp::now() - _ts_last_pull_null).msecval();
      _last_pull_null = false;
    }
    return p;
  } else {
    if ( !_last_pull_null) {
      _last_pull_null = true;
      _ts_last_pull_null = Timestamp::now();
    }
  }

  return NULL;
}

/************************************************************************************************************/
/**************************************** H A N D L E R *****************************************************/
/************************************************************************************************************/

static String
idle_time(Element *e, void */*thunk*/)
{
  return String(((PullStats*)e)->_pull_null_duration);
}

static int
reset_idle_time(const String &/*in_s*/, Element *e, void */*vparam*/, ErrorHandler */*errh*/)
{
  ((PullStats*)e)->_pull_null_duration = 0;
  return 0;
}

void
PullStats::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("idletime", idle_time, (void*)0);
  add_write_handler("reset", reset_idle_time, (void *)0, Handler::BUTTON);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PullStats)
