#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <string>
#include <cstdio>

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
                           uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *, Vector<EtherAddress> *, Vector<EtherAddress> *)
{
  *tx_data_size = 0;

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
  int node_id=1; //Hier muss noch die Richtige gefunden werden
  int first=-1;
  FILE * data = fopen(path,"r");
  int akt;
  bool next=false;
  while (!feof(data)) {
    fscanf(data,"%d",&akt);
    if (first==-1) first=akt;
    if (next==true) {
      if (akt==-1) {
	followers.push_back(first);
      } else {
	followers.push_back(akt);
      }
    }
    if (akt==node_id) {
      next=true;
    }
    if (akt==-1) {
      first=-1;
    }
  }
  fclose(data);
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
