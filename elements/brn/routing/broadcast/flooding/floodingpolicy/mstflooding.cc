#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>

#include "floodingpolicy.hh"
#include "mstflooding.hh"

CLICK_DECLS

MSTFlooding::MSTFlooding()
{
}

MSTFlooding::~MSTFlooding()
{
}

void *
MSTFlooding::cast(const char *name)
{
  if (strcmp(name, "MSTFlooding") == 0)
    return (MSTFlooding *) this;
  else if (strcmp(name, "FloodingPolicy") == 0)
         return (FloodingPolicy *) this;
       else
         return NULL;
}

int
MSTFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
{

  if (cp_va_kparse(conf, this, errh,
    "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
    "CIRCLEPATH", cpkP+cpkM, cpString, &_circle_path,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  get_neighbours(_circle_path);
  return 0;
}

int
MSTFlooding::initialize(ErrorHandler *)
{

  return 0;
}

bool
MSTFlooding::do_forward(EtherAddress *, EtherAddress *, const EtherAddress *, uint32_t, bool is_known, uint32_t,
                           uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *, 
			    Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
  BRN_DEBUG("do_forward reached");
  *tx_data_size = 0;
  for (Vector<int>::iterator i=followers.begin();i!=followers.end();++i) {
    (*unicast_dst).push_back(ID_to_MAC(*i));
    (*passiveack).push_back(ID_to_MAC(*i));
  }
  BRN_DEBUG("NEIGHBOURS %d *unicast_dst.size() %d",followers.size(),(*unicast_dst).size());
  return ! is_known;
}

int
MSTFlooding::policy_id()
{
  return POLICY_ID_MST;
}

void MSTFlooding::get_neighbours(String path) {
  /*Dateiformat:
   * Jeder Kreis eine Zeile
   * Zeilen terminiert durch -1
   * Bsp:
   * 1 2 3 -1
   * 2 4 5 6 7 -1
   */
  
  String _data = file_string(path);
  Vector<String> _data_vec;
  cp_spacevec(_data, _data_vec);
  
  int node_id=0;
  EtherAddress _tmp=ID_to_MAC(node_id);
  while (!((*_me).isIdentical(&_tmp))) {
    node_id++;
    if (node_id>200) {
      BRN_DEBUG("ID not found");
      break;
    }
    _tmp=ID_to_MAC(node_id);
  }
  BRN_DEBUG("ID found (%d)",node_id);
  int first=-1;
  int akt;
  bool next=false;
  int i = 0;

  //BRN_DEBUG("_data_vec.size(): %d",_data_vec.size());
  while (i < _data_vec.size()) {
    cp_integer(_data_vec[i],&akt);
    ++i;
    
    if (first==-1) first=akt;

    if (next) {
      if (akt==-1) {
        followers.push_back(first);
	BRN_DEBUG("Added node: %d",first);
      } else {
        followers.push_back(akt);
	BRN_DEBUG("Added node: %d",akt);
      }
      next=false;
    }

    if (akt==node_id) {
      next=true;
    }

    if (akt==-1) {
      first=-1;
    }
  }
  
  BRN_DEBUG("Neighbours: %d",followers.size());
  
}


EtherAddress MSTFlooding::ID_to_MAC (int id) {
  unsigned char data[]={0,0,0,0,0,0};
  data[5]=id;
  return EtherAddress(data);
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

String
MSTFlooding::flooding_info(void)
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
  MSTFlooding *sfl = (MSTFlooding *)e;

  switch ((uintptr_t) thunk)
  {
    case H_FLOODING_INFO : return ( sfl->flooding_info( ) );
    default: return String();
  }
}

void MSTFlooding::add_handlers()
{
  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MSTFlooding)
