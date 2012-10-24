#include <click/config.h>
#include <click/error.hh>
#include "agefilter.hh"
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/handlercall.hh>
#include <click/variableenv.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

CLICK_DECLS

AgeFilter::AgeFilter()
    : maxage(0)
{
  BRNElement::init();
}

AgeFilter::~AgeFilter()
{
}

int
AgeFilter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
      "MAXAGE", cpkP, cpTimestamp, &maxage,
      "DEBUG", 0, cpInteger, &_debug,
        cpEnd) < 0)
    return -1;

    return 0;
}

int
AgeFilter::initialize(ErrorHandler *)
{
    return 0;
}

Packet *
AgeFilter::kill(Packet *p)
{
  if ( noutputs() > 1 )
    output(1).push(p);
  else
    p->kill();

  return 0;
}

Packet *
AgeFilter::simple_action(Packet *p)
{
  const Timestamp& tv = p->timestamp_anno();

  int age = ((Timestamp::now() - tv).msecval());

  BRN_INFO("Age of Packet: %d ms Max %d ms", age, maxage.msecval());
  if ( maxage.msecval() < ((Timestamp::now() - tv).msecval()) )
    return kill(p);

  return p;
}

void
AgeFilter::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AgeFilter)
