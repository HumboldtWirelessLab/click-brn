#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

#include "floodingpolicy.hh"
#include "simpleflooding.hh"

CLICK_DECLS

SimpleFlooding::SimpleFlooding()
{
}

SimpleFlooding::~SimpleFlooding()
{
}

void *
SimpleFlooding::cast(const char *name)
{
  if (strcmp(name, "SimpleFlooding") == 0)
    return dynamic_cast<SimpleFlooding *>(this);
  else if (strcmp(name, "FloodingPolicy") == 0)
         return dynamic_cast<FloodingPolicy *>(this);
       else
         return NULL;
}

int
SimpleFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
SimpleFlooding::initialize(ErrorHandler *)
{
  return 0;
}

bool
SimpleFlooding::do_forward(EtherAddress *, EtherAddress *, const EtherAddress *, uint32_t, bool is_known, uint32_t,
                           uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *, Vector<EtherAddress> *, Vector<EtherAddress> *)
{
  *tx_data_size = 0;

  return ! is_known;
}

int
SimpleFlooding::policy_id()
{
  return POLICY_ID_SIMPLE;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
SimpleFlooding::flooding_info(void)
{
  StringAccum sa;

  return sa.take_string();
}

enum {
  H_FLOODING_INFO
};

static String
read_param(Element *e, void *thunk)
{
  SimpleFlooding *sfl = reinterpret_cast<SimpleFlooding *>(e);

  switch ((uintptr_t) thunk)
  {
    case H_FLOODING_INFO : return ( sfl->flooding_info( ) );
    default: return String();
  }
}

void SimpleFlooding::add_handlers()
{
  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SimpleFlooding)
