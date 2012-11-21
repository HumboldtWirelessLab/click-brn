#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

#include "floodingpolicy.hh"
#include "multirateflooding.hh"

CLICK_DECLS

MultirateFlooding::MultirateFlooding()
{
  BRNElement::init();
}

MultirateFlooding::~MultirateFlooding()
{
}

void *
MultirateFlooding::cast(const char *name)
{
  if (strcmp(name, "MultirateFlooding") == 0)
    return (MultirateFlooding *) this;
  else if (strcmp(name, "FloodingPolicy") == 0)
         return (FloodingPolicy *) this;
       else
         return NULL;
}

int
MultirateFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
    "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
    "MAXNBMETRIC", cpkP+cpkM, cpInteger, &_max_metric_to_neighbor,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
MultirateFlooding::initialize(ErrorHandler *)
{
  return 0;
}

bool
MultirateFlooding::do_forward(EtherAddress */*src*/, EtherAddress */*fwd*/, const EtherAddress */*rcv*/, uint32_t /*id*/, bool /*is_known*/,
                              uint32_t /*rx_data_size*/, uint8_t */*rxdata*/, uint32_t */*tx_data_size*/, uint8_t */*txdata*/,
                              Vector<EtherAddress> */*unicast_dst*/, Vector<EtherAddress> */*passiveack*/)
{
  BRN_ERROR("No implementation");

  return false;
}

int
MultirateFlooding::policy_id()
{
  return POLICY_ID_MULTIRATE;
}


void
MultirateFlooding::get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out)
{
  if (_link_table) {
    Vector<EtherAddress> neighbors_tmp;

    _link_table->get_neighbors(node, neighbors_tmp);

    for( int n_i = 0; n_i < neighbors_tmp.size(); n_i++) {
        // calc metric between this neighbor and node to make sure that we are well-connected
      int metric_nb_node = _link_table->get_link_metric(node, neighbors_tmp[n_i]);

      // skip to bad neighbors
      if (metric_nb_node > _max_metric_to_neighbor) {
        continue;
      }
      out.push_back(neighbors_tmp[n_i]);
    }
  }
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
MultirateFlooding::flooding_info(void)
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
  MultirateFlooding *sfl = (MultirateFlooding *)e;

  switch ((uintptr_t) thunk)
  {
    case H_FLOODING_INFO : return ( sfl->flooding_info( ) );
    default: return String();
  }
}

void MultirateFlooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MultirateFlooding)
