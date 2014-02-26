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
    "BIDIRECTIONAL", cpkP+cpkM, cpBool, &_bidirectional,
    "ONLY_PRE", cpkP+cpkM, cpBool, &_pre_only,
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

EtherAddress MSTFlooding::ID_to_MAC (int id) {
  unsigned char data[]={0,0,0,0,id/256,id%256};
  return EtherAddress(data);
}

int MSTFlooding::MAC_to_ID(EtherAddress *add) {
  const unsigned char *p = add->data();
  return p[5]+256*p[4];
}

void 
MSTFlooding::init_broadcast(EtherAddress * , uint32_t, uint32_t *tx_data_size, uint8_t *,
                        Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
	BRN_DEBUG("init_broadcast reached");
	do_forward(0,0,0,0,false,0,0,0,tx_data_size,0,unicast_dst,passiveack);
}

bool
MSTFlooding::do_forward(EtherAddress *, EtherAddress *fwd, const EtherAddress *, uint32_t, bool is_known, uint32_t,
                           uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *, Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
  BRN_DEBUG("do_forward reached");
  if (!_pre_only&&is_known ) return false;
  if (_pre_only&&fwd!=0&&find(pre.begin(),pre.end(),*fwd)==pre.end())
	return false;
	
  *tx_data_size = 0;
  unicast_dst->clear();
  passiveack->clear();
  /*Gibt nur den nächsten Nachbarn zurück
  if (akt_foll==followers.end()) {
	  BRN_DEBUG("no more neighbours");
	  return false;
  }
  (*unicast_dst).push_back(ID_to_MAC(*akt_foll));
  (*passiveack).push_back(ID_to_MAC(*akt_foll));
  ++akt_foll;
  return true;*/
  /* Gibt alle Nachbarn zurück */
  for (Vector<EtherAddress>::iterator i = followers.begin(); i!=followers.end(); ++i) {
    unicast_dst->push_back(*i);
    passiveack->push_back(*i);
  }
  BRN_DEBUG("NEIGHBOURS %d *unicast_dst.size() %d", followers.size(), (*unicast_dst).size());

  //return ! is_known;
  if (unicast_dst->empty()) return false;
  return true;
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

  while (!(_me->isIdentical(&_tmp))) {
    node_id++;
    if (node_id > 400) {
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
        followers.push_back(ID_to_MAC(first));
        if (_bidirectional)
			pre.push_back(ID_to_MAC(first));
        BRN_DEBUG("Added node: %d",first);
      } else {
        followers.push_back(ID_to_MAC(akt));
        if (_bidirectional)
			pre.push_back(ID_to_MAC(first));
        BRN_DEBUG("Added node: %d",akt);
      }
      next=false;
    }

    if (akt == node_id) {
      next = true;
    }

    if (akt == -1) {
      first = -1;
    }
  }
	
  //if (_bidirectional) {
	first=-1;
    next=false;
    i = _data_vec.size()-2;
	while (i >= -1) {
		if (i==-1) akt=-1;
		if (i!=-1) cp_integer(_data_vec[i],&akt);
		--i;

		if (first==-1) first=akt;

		if (next) {
			if (akt==-1) {
				pre.push_back(ID_to_MAC(first));
				if (_bidirectional)
					followers.push_back(ID_to_MAC(first));
				BRN_DEBUG("Added node: %d",first);
			} else {
				pre.push_back(ID_to_MAC(first));
				if (_bidirectional)
					followers.push_back(ID_to_MAC(akt));
				BRN_DEBUG("Added node: %d",akt);
			}
			next=false;
		}

		if (akt == node_id) {
			next = true;
		}

		if (akt == -1) {
			first = -1;
		}
	}
  //}
	
  BRN_DEBUG("Neighbours: %d",followers.size());
  //akt_foll=followers.begin();
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
  H_FLOODING_INFO,
  H_DEBUG
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

static int add_follower (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  MSTFlooding *mstfl = (MSTFlooding *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress address;

  if ( !cp_ethernet_address(args[0], &address) ) {
    mstfl->followers.push_back(mstfl->ID_to_MAC(atoi(args[0].c_str())));
  }

  return 0;
}

void MSTFlooding::add_handlers()
{
  add_read_handler("flooding_info", read_param , (void *)H_FLOODING_INFO);
  add_write_handler("add_follower", add_follower , (void *) H_DEBUG);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(MSTFlooding)
