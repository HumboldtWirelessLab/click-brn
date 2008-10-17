#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_null.hh"
CLICK_DECLS

BRN2Null::BRN2Null()
{
}

BRN2Null::~BRN2Null()
{
}

int
BRN2Null::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpOptional,
      cpString, "label", &_label,
      cpEnd) < 0)
       return -1;
  return 0;
}

int
BRN2Null::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
BRN2Null::simple_action(Packet *p_in)
{
  return p_in;
}


static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2Null::add_handlers()
{
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Null)
