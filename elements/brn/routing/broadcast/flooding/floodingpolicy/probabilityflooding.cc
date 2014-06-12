
#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>

#include "../flooding.hh"
#include "floodingpolicy.hh"
#include "probabilityflooding.hh"

CLICK_DECLS

ProbabilityFlooding::ProbabilityFlooding():
  _min_no_neighbors(0),
  _fwd_probability(100),
  _max_metric_to_neighbor(5000),
  _cntbased_min_neighbors_for_abort(0)
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
    "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
    "MINNEIGHBOURS", cpkP+cpkM, cpInteger, &_min_no_neighbors,
    "FWDPROBALILITY", cpkP+cpkM, cpInteger, &_fwd_probability,
    "MAXNBMETRIC", cpkP+cpkM, cpInteger, &_max_metric_to_neighbor,
    "CNTNB2ABORT", cpkP+cpkM, cpInteger, &_cntbased_min_neighbors_for_abort,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
ProbabilityFlooding::initialize(ErrorHandler *)
{
  click_brn_srandom();
  return 0;
}

bool
ProbabilityFlooding::do_forward(EtherAddress *src, EtherAddress *, const EtherAddress *, uint32_t bcast_id, bool is_known, uint32_t fwd_cnt, uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *,
                                Vector<EtherAddress> *, Vector<EtherAddress> *)
{
  *tx_data_size = 0;

  if (is_known) {
    /** node is already known;
     * Did we fwd the msg ?
     *
     * We maybe already forward the msg. If so, check wether enough neighbour have the msg and if we didn't send the msg yet->abort!
     *
     */
    if ((fwd_cnt > 0) && (_cntbased_min_neighbors_for_abort > 0)) { //we forward the msg; should we abort?

      struct BroadcastNode::flooding_last_node *last_nodes; //get all nodes which has potential received the msg
      uint32_t last_nodes_size;

      last_nodes = _flooding_db->get_last_nodes(src, bcast_id, &last_nodes_size);

      uint32_t no_rx_nodes = 0; //count nodes from which we received the packet

      for( uint32_t i = 0; i < last_nodes_size; i++ ) {
        if ( last_nodes[i].received_cnt > 0 ) no_rx_nodes++; //inc no_rx_nodes if we received the msg from this node
      }

      if ( no_rx_nodes >= _cntbased_min_neighbors_for_abort ) {
        //stop it
        BroadcastNode *bcn = _flooding_db->get_broadcast_node(src);
        if ( bcn != NULL ) {
          bcn->set_stopped(bcast_id,true);
        }

        if (_flooding->is_last_tx_id(*src, bcast_id)) _flooding->abort_last_tx(FLOODING_TXABORT_REASON_FINISHED);
      }
    }

    return false;
  }

  /**  ----------  Probability stuff -----------
   *
   * Check no. neighbours (NON)
   * if NON > threshhold ) send with P()
   * else send;
   *
   */
  const EtherAddress *me = _me->getMasterAddress();
  CachedNeighborsMetricList* cnml = _fhelper->get_filtered_neighbors(*me);

  BRN_DEBUG("NBs: %d min: %d",cnml->_neighbors.size(),(int32_t)_min_no_neighbors);

  if ( cnml->_neighbors.size() <= (int32_t)_min_no_neighbors ) return true;;

  uint32_t r = (click_random() % 100);

  BRN_DEBUG("Known: %d prob: %d",(is_known?(int)1:(int)0),r);

  return (r < _fwd_probability);
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
