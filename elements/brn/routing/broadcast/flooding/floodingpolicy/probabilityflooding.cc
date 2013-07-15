
#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

#include "floodingpolicy.hh"
#include "probabilityflooding.hh"

CLICK_DECLS

ProbabilityFlooding::ProbabilityFlooding():
  _min_no_neighbors(0),
  _fwd_probability(100),
  _max_metric_to_neighbor(5000)
{
  BRNElement::init();
}

ProbabilityFlooding::~ProbabilityFlooding()
{
}

void *
ProbabilityFlooding::cast(const char *name)
{
  if (strcmp(name, "ProbabilityFlooding") == 0)
    return (ProbabilityFlooding *) this;
  else if (strcmp(name, "FloodingPolicy") == 0)
         return (FloodingPolicy *) this;
       else
         return NULL;
}

int
ProbabilityFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
    "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
    "MINNEIGHBOURS", cpkP+cpkM, cpInteger, &_min_no_neighbors,
    "FWDPROBALILITY", cpkP+cpkM, cpInteger, &_fwd_probability,
    "MAXNBMETRIC", cpkP+cpkM, cpInteger, &_max_metric_to_neighbor,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
ProbabilityFlooding::initialize(ErrorHandler *)
{
  click_srandom(_me->getMasterAddress()->hashcode());
  return 0;
}

bool
ProbabilityFlooding::do_forward(EtherAddress *, EtherAddress *, const EtherAddress *, uint32_t, bool is_known, uint32_t, uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *,
                                Vector<EtherAddress> *, Vector<EtherAddress> *)
{
  *tx_data_size = 0;

  const EtherAddress *me = _me->getMasterAddress();
  CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(*me);
  
  BRN_DEBUG("NBs: %d min: %d",cnml->_neighbors.size(),(int32_t)_min_no_neighbors);
  
  if ( cnml->_neighbors.size() <= (int32_t)_min_no_neighbors ) return !is_known;

  uint32_t r = (click_random() % 100);
  
  BRN_DEBUG("Known: %d prob: %d",(is_known?(int)1:(int)0),r);
  
  return !is_known && (r < _fwd_probability);
}

int
ProbabilityFlooding::policy_id()
{
  return POLICY_ID_PROBABILITY;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
ProbabilityFlooding::flooding_info(void)
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
  ProbabilityFlooding *sfl = (ProbabilityFlooding *)e;

  switch ((uintptr_t) thunk)
  {
    case H_FLOODING_INFO : return ( sfl->flooding_info( ) );
    default: return String();
  }
}

void ProbabilityFlooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ProbabilityFlooding)
