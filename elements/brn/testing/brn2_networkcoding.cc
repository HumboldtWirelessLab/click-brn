#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_networkcoding.hh"

//#include "gmp.h"

CLICK_DECLS

BRN2Networkcoding::BRN2Networkcoding()
{
}

BRN2Networkcoding::~BRN2Networkcoding()
{
}

int
BRN2Networkcoding::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      cpOptional,
      cpString, "label", &_label,
      cpEnd) < 0)
       return -1;
  return 0;
}

int
BRN2Networkcoding::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
BRN2Networkcoding::simple_action(Packet *p_in)
{
//  mpz_t integ;

//  mpz_init(integ);
//  mpz_clear(integ);
  return p_in;
}


static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2Networkcoding::add_handlers()
{
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2Networkcoding)
//ELEMENT_LIBS(-lgmp -lgmpxx)
