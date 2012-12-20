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

CLICK_DECLS

MPRFlooding::MPRFlooding():
  _update_interval(MPR_DEFAULT_UPDATE_INTERVAL),
  _fix_mpr(false)
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
    "MPRUPDATEINTERVAL",cpkP, cpInteger, &_update_interval,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
MPRFlooding::initialize(ErrorHandler *)
{
  _last_set_mpr_call = Timestamp::now();

  return 0;
}

void
MPRFlooding::init_broadcast(EtherAddress *, uint32_t, uint32_t *tx_data_size, uint8_t *txdata, Vector<EtherAddress> *, Vector<EtherAddress> *)
{
  if ((_mpr_forwarder.size() == 0) || //if we have now mpr
      ((!_fix_mpr) &&                 //or we dont use fixed mprs & need update
	(Timestamp::now() - _last_set_mpr_call).msecval() > _update_interval)) {
      set_mpr();
  }

  if ( *tx_data_size < (uint32_t)(6 * _mpr_forwarder.size()) ) {  //i have more mprs than expected 
    BRN_ERROR("MPRHeader: no space left");
    memcpy(txdata, brn_ethernet_broadcast, 6);         //every neighbour should forward
    *tx_data_size = 6;
  } else {
    for ( int a = 0, index = 0 ; a < _mpr_forwarder.size(); a++, index+=6 )
      memcpy(&(txdata[index]), _mpr_forwarder[a].data(), 6);

    *tx_data_size = 6 * _mpr_forwarder.size();
  }
}

bool
MPRFlooding::do_forward(EtherAddress */*src*/, EtherAddress */*fwd*/, const EtherAddress */*rcv*/, uint32_t /*id*/, bool /*is_known*/, uint32_t forward_count,
                        uint32_t rx_data_size, uint8_t *rxdata, uint32_t *tx_data_size, uint8_t *txdata,
                        Vector<EtherAddress> */*unicast_dst*/, Vector<EtherAddress> */*passiveack*/)
{
  EtherAddress ea;
  uint32_t i;

  if ( forward_count > 0 ) return false;

  for(i = 0; i < rx_data_size; i +=6) {
    ea = EtherAddress(&(rxdata[i]));
    if ( _me->isIdentical(&ea) || (ea == brn_etheraddress_broadcast)) break;
  }

  if ( i <= rx_data_size ) { //i'm a mpr
    if ((_mpr_forwarder.size() == 0) || //if we have now mpr
        ((!_fix_mpr) &&                 //or we dont use fixed mprs & need update
         (Timestamp::now() - _last_set_mpr_call).msecval() > _update_interval)) {
       set_mpr();
    }

    //i have more mprs than expected or none
    if ( (*tx_data_size < (uint32_t)(6 * _mpr_forwarder.size())) || (_mpr_forwarder.size() == 0)) { 
      BRN_ERROR("MPRHeader: no space left(%d) or no forwarder (%d)",(6 * _mpr_forwarder.size()),_mpr_forwarder.size());
      memcpy(txdata, brn_ethernet_broadcast, 6);         //every neighbour should forward
      *tx_data_size = 6;
    } else {
      for ( int a = 0, index = 0 ; a < _mpr_forwarder.size(); a++, index+=6 )
        memcpy(&(txdata[index]), _mpr_forwarder[a].data(), 6);

      *tx_data_size = 6 * _mpr_forwarder.size();
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
MPRFlooding::set_mpr()
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

  if (_link_table) {
    //
    //get neighbours
    //
    const EtherAddress *me = _me->getMasterAddress();
    get_filtered_neighbors(*me, neighbors);

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
      get_filtered_neighbors(neighbors[n_i], nb_neighbors);

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
	      
	    delete adj_mat;
	    adj_mat = new_adj_mat;
	  }
	
	  adj_mat_used++;
	  
	} else {
	  adj_mat_index = neighbour_map.find(nb_neighbors[n_j]);
	  if ( adj_mat_index >= count_one_hop_nbs ) {           //is a two hop neighbour
	    int *cov_count = uncovered.findp(nb_neighbors[n_j]);
	    (*cov_count)++;
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
  }
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
  int result = set_mpr_vector(in_s, &_mpr_forwarder);
  _fix_mpr = (_mpr_forwarder.size() != 0);

  return result;
}

int
MPRFlooding::set_mpr_destination(const String &in_s)
{
  return set_mpr_vector(in_s, &_mpr_unicast);
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
    case H_MPRFLOODING_GET_MPR : {
                                    mprfl->set_mpr();
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
  add_write_handler("destination", write_param, (void *) H_MPRFLOODING_DESTINATION);
  add_write_handler("mpr_algo", write_param, (void *) H_MPRFLOODING_GET_MPR);
  
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MPRFlooding)
