#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>


#include "elements/brn/brn2.h"
#include "floodingpolicy.hh"
#include "mprflooding.hh"
#include "../flooding.hh"


CLICK_DECLS

MPRFlooding::MPRFlooding():
  _me(NULL),_fhelper(NULL),_flooding_db(NULL),
  _max_metric_to_neighbor(MPR_DEFAULT_NB_METRIC),
  _min_pdr_to_neighbor(0),
  _update_interval(MPR_DEFAULT_UPDATE_INTERVAL),
  _fix_mpr(false),
  _last_bcast_id(-1),
  _remove_finished_nodes(false)
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
    return dynamic_cast<MPRFlooding *>(this);
  else if (strcmp(name, "FloodingPolicy") == 0)
         return dynamic_cast<FloodingPolicy *>(this);
       else
         return NULL;
}

int
MPRFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
    "FLOODINGHELPER", cpkP+cpkM, cpElement, &_fhelper,
    "FLOODINGDB", cpkP+cpkM, cpElement, &_flooding_db,
    "MAXNBMETRIC", cpkP+cpkM, cpInteger, &_max_metric_to_neighbor,
    "MPRUPDATEINTERVAL",cpkP, cpInteger, &_update_interval,
    "REMOVEFINISHEDNODES",cpkP, cpBool, &_remove_finished_nodes,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  if ( _max_metric_to_neighbor == 0 ) _min_pdr_to_neighbor = 100;
  else if ( _max_metric_to_neighbor == BRN_LT_INVALID_LINK_METRIC ) _min_pdr_to_neighbor = 0;
  else _min_pdr_to_neighbor = (1000 / isqrt32(_max_metric_to_neighbor));

  return 0;
}

int
MPRFlooding::initialize(ErrorHandler *)
{
  _last_set_mpr_call = Timestamp::now();

  return 0;
}

/**
 *
 * @bcast_id: negatvive value means: don't filter known neighbours
 */
void
MPRFlooding::set_mpr_header(uint32_t *tx_data_size, uint8_t *txdata, EtherAddress *src, int bcast_id)
{
  if ( bcast_id == -1 ) {
    if ((_mpr_forwarder.size() == 0) || //if we have no mpr
        ((!_fix_mpr) &&                 //or we dont use fixed mprs & need update
         (Timestamp::now() - _last_set_mpr_call).msecval() > _update_interval) ||
         (_last_bcast_id != -1)) {
        set_mpr(NULL);
    }
  } else {
    HashMap<EtherAddress,EtherAddress> known_nodes;

    struct BroadcastNode::flooding_node_info *last_nodes;

    uint32_t last_nodes_size;
    last_nodes = _flooding_db->get_node_infos(src, bcast_id, &last_nodes_size);

    for ( uint32_t j = 0; j < last_nodes_size; j++ ) {                                     //add node to candidate set if
      if ( ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_FINISHED) != 0) ||             //1. is known as acked
           ((last_nodes[j].flags & FLOODING_NODE_INFO_FLAGS_FOREIGN_RESPONSIBILITY) != 0)) //2. foreign respons
          known_nodes.insert(EtherAddress(last_nodes[j].etheraddr),EtherAddress(last_nodes[j].etheraddr));
    }

    set_mpr(&known_nodes);
    known_nodes.clear();
  }

  _last_bcast_id = bcast_id;

  if ( (*tx_data_size < (uint32_t)(sizeof(struct click_brn_bcast_extra_data) + (6 * _mpr_forwarder.size()))) || (_mpr_forwarder.size() == 0)) { 
    BRN_ERROR("MPRHeader: no space left");
    *tx_data_size = 0;                          //every neighbour should forward
  } else {
    for ( int a = 0, index = sizeof(struct click_brn_bcast_extra_data); a < _mpr_forwarder.size(); a++, index+=6 )
      memcpy(&(txdata[index]), _mpr_forwarder[a].data(), 6);

    struct click_brn_bcast_extra_data *extdat = (struct click_brn_bcast_extra_data *)txdata;
    extdat->size = (uint8_t)(sizeof(struct click_brn_bcast_extra_data) + (6 * _mpr_forwarder.size()));
    extdat->type = (uint8_t)BCAST_EXTRA_DATA_MPR;

    *tx_data_size = sizeof(struct click_brn_bcast_extra_data) + (6 * _mpr_forwarder.size());
  }
}


void
MPRFlooding::init_broadcast(EtherAddress *, uint32_t, uint32_t *tx_data_size, uint8_t *txdata,
                            Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack)
{
  set_mpr_header(tx_data_size, txdata, NULL, -1);

  int n = 1;

  for (Vector<EtherAddress>::iterator i = _mpr_forwarder.begin(); i!=_mpr_forwarder.end(); ++i, n++) {
    BRN_ERROR("INIT: NODE %d: %s",n,i->unparse().c_str());
    unicast_dst->push_back(*i);
    passiveack->push_back(*i);
  }
}

bool
MPRFlooding::do_forward(EtherAddress *src, EtherAddress */*fwd*/, const EtherAddress */*rcv*/, uint32_t id, bool /*is_known*/,
                        uint32_t forward_count, uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                        Vector<EtherAddress> *unicast_dst, Vector<EtherAddress> *passiveack)
{
  bool fwd = false;

  if ( forward_count > 0 ) return false;

  BRN_DEBUG("do_fwd: %d", rx_data_size);

  if ( rx_data_size == 0 ) fwd = true;
  else {
    uint32_t i = 0;

    for(; (i < rx_data_size) && (fwd == false);) {

      struct click_brn_bcast_extra_data *extdat = (struct click_brn_bcast_extra_data *)&(rxdata[i]);

      BRN_DEBUG("i: %d Type: %d Size: %d",i, (uint32_t)extdat->type, (uint32_t)extdat->size);

      if ( extdat->type == BCAST_EXTRA_DATA_MPR ) {
        BRN_DEBUG("Found MPR stuff: Size: %d",extdat->size);

        uint32_t rxdata_idx = i + sizeof(struct click_brn_bcast_extra_data);
        uint32_t mpr_data_idx = sizeof(struct click_brn_bcast_extra_data);

        for(;mpr_data_idx < extdat->size; mpr_data_idx += 6, rxdata_idx += 6 ) {
          EtherAddress ea = EtherAddress(&(rxdata[rxdata_idx]));
          if ( _me->isIdentical(&ea) || (ea == brn_etheraddress_broadcast)) {
            fwd = true;
            break;
          }
        }
        break;
      } else i += extdat->size;
    }

    assert(i <= rx_data_size);

    if ( i == rx_data_size ) fwd = true; //no BCAST_EXTRA_DATA_MPR found, so forward to be sure
    else if ( i > rx_data_size ) {       //header is corrupted -> forward to be sure
      BRN_ERROR("Bcast-Header-Error");
      fwd = true;
    }
  }

  if (fwd) { //i'm a mpr
    set_mpr_header(tx_data_size, txdata, src, (_remove_finished_nodes?id:-1));

    for (Vector<EtherAddress>::iterator i = _mpr_forwarder.begin(); i!=_mpr_forwarder.end(); ++i) {
      unicast_dst->push_back(*i);
      passiveack->push_back(*i);
    }

    return true;
  }

  return false;
}

int
MPRFlooding::policy_id()
{
  return POLICY_ID_MPR;
}

void
MPRFlooding::set_mpr(HashMap<EtherAddress,EtherAddress> *known_nodes)
{
  Vector<EtherAddress> neighbors;

  HashMap<EtherAddress, int> neighbour_map;
  HashMap<EtherAddress, int> uncovered;

  HashMap<EtherAddress, EtherAddress> mpr_nodes;

  uint8_t *adj_mat;
  uint16_t adj_mat_size = 16;
  uint16_t adj_mat_used = 0;

  uint16_t count_one_hop_nbs = 0;

  _last_set_mpr_call = Timestamp::now();

  _mpr_forwarder.clear();
  _neighbours.clear();

  //
  //get neighbours
  //
  const EtherAddress *me = _me->getMasterAddress();
  _fhelper->get_filtered_neighbors(*me, neighbors, _min_pdr_to_neighbor);
  if ( known_nodes != NULL ) remove_finished_neighbours(&neighbors, known_nodes);

  if (neighbors.size() == 0) {
    BRN_DEBUG("No neighbours, so no mprs.");
    return;
  }

  if ( neighbors.size() > 4 )
    adj_mat_size = neighbors.size() * neighbors.size();

  adj_mat = new uint8_t[adj_mat_size*adj_mat_size]; //Matrix
  memset(adj_mat, 0, adj_mat_size*adj_mat_size);

  count_one_hop_nbs = adj_mat_used = neighbors.size();

  for( int n_i = 0; n_i < neighbors.size(); n_i++) {
    neighbour_map.insert(neighbors[n_i],n_i);
    _neighbours.push_back(neighbors[n_i]);
  }

  //
  // get 2-hop neighbours
  //
  for( int n_i = 0; n_i < count_one_hop_nbs; n_i++) { // iterate over all my neighbors

    Vector<EtherAddress> nb_neighbors;               // the neighbors of my neighbors
    _fhelper->get_filtered_neighbors(neighbors[n_i], nb_neighbors, _min_pdr_to_neighbor);
    if ( known_nodes != NULL ) remove_finished_neighbours(&nb_neighbors, known_nodes);

    for( int n_j = 0; n_j < nb_neighbors.size(); n_j++) { // iterate over all my neighbors neighbours

      int adj_mat_index;

      if ( neighbour_map.findp(nb_neighbors[n_j]) == NULL ) {

        adj_mat_index = adj_mat_used;
        neighbour_map.insert(nb_neighbors[n_j],adj_mat_index);
        uncovered.insert(nb_neighbors[n_j],1);
        neighbors.push_back(nb_neighbors[n_j]);

        if ( adj_mat_used == adj_mat_size ) {
          adj_mat_size *= 2;
          uint8_t *new_adj_mat = new uint8_t[adj_mat_size*adj_mat_size]; //Matrix
          memset(new_adj_mat, 0, adj_mat_size*adj_mat_size);

          //copy old matrix
          for ( int i = 0; i < adj_mat_index; i++ )
            for ( int j = 0; j < adj_mat_index; j++ )
              new_adj_mat[i*adj_mat_size + j] = adj_mat[i*adj_mat_used + j];

          delete[] adj_mat;
          adj_mat = new_adj_mat;
        }

        adj_mat_used++;

      } else {
        adj_mat_index = neighbour_map.find(nb_neighbors[n_j]);
        if ( adj_mat_index >= count_one_hop_nbs ) {            //is a two hop neighbour
          int *cov_count = uncovered.findp(nb_neighbors[n_j]); //this two hop neighbors is covered by
          (*cov_count)++;                                      //one more 1-hop-neighbor
        }
      }

      adj_mat[n_i*adj_mat_size + adj_mat_index] = 1;
      adj_mat[adj_mat_index*adj_mat_size + n_i] = 1;
    }
  }

  //
  // DEBUG
  //
  if ( BRN_DEBUG_LEVEL_DEBUG ) {
    StringAccum sa;
    sa << "Neighbours: " << count_one_hop_nbs << " 2hop-neighbours: " << uncovered.size() <<"\n";
    for ( int y = 0; y < adj_mat_used;y++) {  
      for ( int z = 0; z < adj_mat_used;z++) {
        sa << (int)adj_mat[y*adj_mat_size + z] << " ";
      }
      sa << "\n";
    }

    click_chatter("%s", sa.take_string().c_str());
  }

  //found one hop neighbours who are single cov for 2-hop-neighbour

  for( int i = count_one_hop_nbs; i < adj_mat_used; i++) {  //check all 2-hop neighbours
    EtherAddress ea = neighbors[i];

    if ( uncovered.findp(ea) == NULL ) continue;            //already coverd

    uint8_t n = uncovered.find(ea);
    if ( n == 1 ) {                                         //covered only once
      BRN_DEBUG("Covered by one: %s",ea.unparse().c_str());
      uint8_t node_id = neighbour_map.find(ea);
      uint8_t conn_count = 0;
      uint16_t one_hop_neighbour = 0;
      for ( int a = 0; a < count_one_hop_nbs; a++ ) {
        if ( adj_mat[a*adj_mat_size + node_id] == 1 ) {
          one_hop_neighbour = a;   
          conn_count++;
        }
      }

      if ( conn_count == 1 ) {                             // 2 hop node only covered by onre hop node one_hop_neighbour
        uncovered.erase(ea);                               //node is coverd by one node (mpr)
        _mpr_forwarder.push_back(neighbors[one_hop_neighbour]); //node is mpr
        mpr_nodes.insert(neighbors[one_hop_neighbour],neighbors[one_hop_neighbour]);

        //delete edges to node
        for ( int x = 0; x < adj_mat_size;x++) {
          adj_mat[x*adj_mat_size + i] = 0;
          adj_mat[i*adj_mat_size + x] = 0;
        }

        for( int j = count_one_hop_nbs; j < adj_mat_used; j++ ) {  //check for other nodes coverd by one hop node
          if ( adj_mat[j*adj_mat_size + one_hop_neighbour] == 1 ) {

            //every node coverd by mpr can be deleted
            for ( int x = 0; x < adj_mat_size;x++) {
              adj_mat[x*adj_mat_size + j] = 0;
              adj_mat[j*adj_mat_size + x] = 0;
            }

            EtherAddress two_hop_nb = neighbors[j];
            if ( uncovered.findp(two_hop_nb) != NULL )
              uncovered.erase(two_hop_nb);
          }
        }
      }
    } else {
      BRN_DEBUG("Covered by %d nodes",(int)n);
    }
  }

  for( int m = 0; m < _mpr_forwarder.size(); m++ ) {
    BRN_DEBUG("MPR: %s", _mpr_forwarder[m].unparse().c_str());
  }

  //check for one hop nodes which covers most of 2 hop nbbs
  int unc_s = uncovered.size();
  while ( unc_s != 0 ) {
    EtherAddress max_ea = EtherAddress::make_broadcast();
    uint16_t max_count = 0;
    uint16_t max_id = 0;

    for( int g = count_one_hop_nbs; g < adj_mat_used; g++ ) {
      if ( uncovered.findp(neighbors[g]) != NULL ) {
        BRN_DEBUG("Uncovered node: %s",neighbors[g].unparse().c_str());
      }
    }

    for( int i = 0; i < count_one_hop_nbs; i++) {  //check all 1-hop neighbours
      EtherAddress ea = neighbors[i];
      if ( mpr_nodes.findp(ea) != NULL ) continue;

      uint16_t cnt_2hop_nb = 0;

      BRN_DEBUG("Non mpr node: %s %d %d",ea.unparse().c_str(),max_id,cnt_2hop_nb);

      for( int j = count_one_hop_nbs; j < adj_mat_used; j++ ) {
        if ( adj_mat[j*adj_mat_size + i] == 1 ) cnt_2hop_nb++;
        else if ( adj_mat[i*adj_mat_size + j] == 1 ) cnt_2hop_nb++;
      }

      if ( cnt_2hop_nb > max_count ) {
        max_ea = ea;
        max_count = cnt_2hop_nb;
        max_id = i;
      }
    }

    BRN_DEBUG("MaxCount: %d", max_count);

    if ( max_count == 0 ) {
      BRN_DEBUG("Error");
    } else {
      _mpr_forwarder.push_back(max_ea); //node is mpr
      mpr_nodes.insert(max_ea,max_ea);

      for( int j = count_one_hop_nbs; j < adj_mat_used; j++ ) {  //check for other nodes coverd by one hop node
        if ( adj_mat[j*adj_mat_size + max_id ] == 1 ) {

          for ( int x = 0; x < adj_mat_size;x++) {             //2hop node is coverd: delete edges
            adj_mat[x*adj_mat_size + j] = 0;
            adj_mat[j*adj_mat_size + x] = 0;
          }

          EtherAddress two_hop_nb = neighbors[j];
          if ( uncovered.findp(two_hop_nb) != NULL )
            uncovered.erase(two_hop_nb);
        }
      }
    }

    unc_s--;
  }

  delete[] adj_mat;
}

void
MPRFlooding::remove_finished_neighbours(Vector<EtherAddress> *neighbors, HashMap<EtherAddress, EtherAddress> *known_nodes)
{
  for( int i = neighbors->size()-1; i >= 0; i--)
    if ( known_nodes->findp((*neighbors)[i]) != NULL )
       neighbors->erase(neighbors->begin() + i);
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
  int result = set_mpr_vector(in_s, &_mpr_forwarder);
  _fix_mpr = (_mpr_forwarder.size() != 0);

  return result;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
MPRFlooding::flooding_info(void)
{
  StringAccum sa;

  sa << "<mprflooding node=\"" << _me->getMasterAddress()->unparse().c_str() << "\" >\n";

  sa << "\t<mprset size=\"" << _mpr_forwarder.size() << "\" >\n";
  for( int i = 0; i < _mpr_forwarder.size(); i++ ) {
    sa << "\t\t<mpr node=\"" << _mpr_forwarder[i].unparse().c_str() << "\" />\n";
  }
  sa << "\t</mprset>\n";

  sa << "\t<neighbourset size=\"" << _neighbours.size() << "\" >\n";
  for( int i = 0; i < _neighbours.size(); i++ ) {
    sa << "\t\t<neighbour node=\"" << _neighbours[i].unparse().c_str() << "\" />\n";
  }
  sa << "\t</neighbourset>\n";

  sa << "</mprflooding>\n";

  return sa.take_string();
}

enum {
  H_FLOODING_INFO,
  H_MPRFLOODING_FORWARDER,
  H_MPRFLOODING_DESTINATION,
  H_MPRFLOODING_GET_MPR
};

static String
read_param(Element *e, void *thunk)
{
  MPRFlooding *mprfl = reinterpret_cast<MPRFlooding *>(e);

  switch ((uintptr_t) thunk)
  {
    case H_FLOODING_INFO : return ( mprfl->flooding_info() );
    default: return String();
  }
}

static int
write_param(const String &in_s, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  MPRFlooding *mprfl = reinterpret_cast<MPRFlooding *>(e);
  String s = cp_uncomment(in_s);

  switch ((uintptr_t) vparam)
  {
    case H_MPRFLOODING_FORWARDER : return ( mprfl->set_mpr_forwarder(in_s) );
    case H_MPRFLOODING_GET_MPR : {
                                    mprfl->set_mpr(NULL);
                                    return 0;
                                 }
    default: return 0;
  }

  return 0;
}

void MPRFlooding::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);

  add_write_handler("forwarder", write_param, (void *) H_MPRFLOODING_FORWARDER);
  add_write_handler("mpr_algo", write_param, (void *) H_MPRFLOODING_GET_MPR);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(MPRFlooding)
