#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>

#include "floodingpolicy.hh"
#include "overlaypolicy.hh"

CLICK_DECLS

OverlayPolicy::OverlayPolicy()
{
}

OverlayPolicy::~OverlayPolicy()
{
}

void *
OverlayPolicy::cast(const char *name)
{
  if (strcmp(name, "OverlayPolicy") == 0)
    return (OverlayPolicy *) this;
  else if (strcmp(name, "FloodingPolicy") == 0)
         return (FloodingPolicy *) this;
       else
         return NULL;
}

int
OverlayPolicy::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
    "OVERLAY_STRUCTURE", cpkP+cpkM, cpElement, &_ovl,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
OverlayPolicy::initialize(ErrorHandler *)
{

  return 0;
}

void 
OverlayPolicy::init_broadcast(EtherAddress * , uint32_t, uint32_t *tx_data_size, uint8_t *,
                        Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
	//BRN_DEBUG("init_broadcast reached");
	do_forward(0,0,0,0,false,0,0,0,tx_data_size,0,unicast_dst,passiveack);
}

bool
OverlayPolicy::do_forward(EtherAddress *, EtherAddress *, const EtherAddress *, uint32_t, bool is_known, uint32_t,
                           uint32_t, uint8_t *, uint32_t *tx_data_size, uint8_t *, Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
  //BRN_DEBUG("do_forward reached");
  if (is_known) return false;
  Vector<EtherAddress> * children=_ovl->getOwnChildren();
  if (children->empty())
	return false;
  for (Vector<EtherAddress>::iterator i=children->begin();i!=children->end();++i) {
	 unicast_dst->push_back(*i);
     passiveack->push_back(*i);
  }
  return true;
}

int
OverlayPolicy::policy_id()
{
  return POLICY_ID_OVERLAY;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/


void OverlayPolicy::add_handlers()
{
	
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OverlayPolicy)
