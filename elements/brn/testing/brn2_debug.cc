#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_debug.hh"
CLICK_DECLS

BRN2Debug::BRN2Debug()
{
}

BRN2Debug::~BRN2Debug()
{
}

int
BRN2Debug::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpOptional,
      cpInteger, "debug", &_debug,
      cpEnd) < 0)
       return -1;
  return 0;
}

int
BRN2Debug::initialize(ErrorHandler *)
{
  _debug = 0;
  return 0;
}

static int 
write_handler_debug(const String &in_s, Element *e, void *,ErrorHandler *errh)
{
  BRN2Debug *brn2debug = (BRN2Debug*)e;
  String s = cp_uncomment(in_s);

  int debug;

  if (!cp_integer(s, &debug))
    return errh->error("startup_time parameter must be an integer");

  brn2debug->_debug = debug;

  return 0;
}

/**
 * Read the debugvalue
 * @param e 
 * @param  
 * @return 
 */
static String
read_handler_debug(Element *e, void *)
{
  BRN2Debug *brn2debug = (BRN2Debug*)e;

  return String(brn2debug->_debug);
}

void
BRN2Debug::add_handlers()
{
  add_read_handler("debug", read_handler_debug, 0);
  add_write_handler("debug", write_handler_debug, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Debug)
