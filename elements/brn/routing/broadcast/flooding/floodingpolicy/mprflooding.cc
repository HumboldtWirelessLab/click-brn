#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

#include "floodingpolicy.hh"
#include "mprflooding.hh"

CLICK_DECLS

MPRFlooding::MPRFlooding()
{
  BRNElement::init();
}

MPRFlooding::~MPRFlooding()
{
}

void *
MPRFlooding::cast(const char *name)
{
  if (strcmp(name, "MPRFlooding") == 0)
    return (MPRFlooding *) this;
  else if (strcmp(name, "FloodingPolicy") == 0)
         return (FloodingPolicy *) this;
       else
         return NULL;
}

int
MPRFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
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
MPRFlooding::initialize(ErrorHandler *)
{
  return 0;
}

bool
MPRFlooding::do_forward(EtherAddress */*src*/, EtherAddress */*fwd*/, const EtherAddress */*rcv*/, uint32_t /*id*/, bool is_known,
                        uint32_t /*rx_data_size*/, uint8_t */*rxdata*/, uint32_t */*tx_data_size*/, uint8_t */*txdata*/,
                        Vector<EtherAddress> */*unicast_dst*/, Vector<EtherAddress> */*passiveack*/)
{
  click_random_srandom();

  const EtherAddress *me = _me->getMasterAddress();
  Vector<EtherAddress> nb_neighbors;                    // the neighbors of my neighbors
  get_filtered_neighbors(*me, nb_neighbors);

  if ( nb_neighbors.size() < 3 ) return ! is_known;
  if ( ( click_random() % (nb_neighbors.size() - 2) ) == 1 ) return false;

  return ! is_known;
}

int
MPRFlooding::policy_id()
{
  return POLICY_ID_MPR;
}


void
MPRFlooding::get_filtered_neighbors(const EtherAddress &node, Vector<EtherAddress> &out)
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


int
MPRFlooding::set_mpr_vector(const String &in_s, Vector<EtherAddress> *ea_vector)
{
  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  ea_vector->clear();

  EtherAddress ea;
  
  for ( int i = 0; i < ea_vector->size(); i++ ) {
     cp_ethernet_address(args[i], &ea);
     ea_vector->push_back(ea);
  }

  return 0;
}

int
MPRFlooding::set_mpr_forwarder(const String &in_s)
{
  return set_mpr_vector(in_s, &mpr_forwarder);
}

int
MPRFlooding::set_mpr_destination(const String &in_s)
{
  return set_mpr_vector(in_s, &mpr_unicast);
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
MPRFlooding::flooding_info(void)
{
  StringAccum sa;

  return sa.take_string();
}

enum {
  H_FLOODING_INFO,
  H_MPRFLOODING_FORWARDER,
  H_MPRFLOODING_DESTINATION
};

static String
read_param(Element *e, void *thunk)
{
  MPRFlooding *mprfl = (MPRFlooding *)e;

  switch ((uintptr_t) thunk)
  {
    case H_FLOODING_INFO : return ( mprfl->flooding_info() );
    default: return String();
  }
}

static int
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  MPRFlooding *mprfl = (MPRFlooding *)e;
  String s = cp_uncomment(in_s);

  switch ((uintptr_t) vparam)
  {
    case H_MPRFLOODING_FORWARDER : return ( mprfl->set_mpr_forwarder(in_s) );
    case H_MPRFLOODING_DESTINATION : return ( mprfl->set_mpr_destination(in_s) );
    default: return 0;
  }

  return 0;
}

void MPRFlooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);

  add_write_handler("forwarder", write_param, (void *) H_MPRFLOODING_FORWARDER);
  add_write_handler("destination", write_param, (void *) H_MPRFLOODING_DESTINATION);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(MPRFlooding)
