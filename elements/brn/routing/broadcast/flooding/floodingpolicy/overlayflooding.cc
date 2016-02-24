#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>

#include "floodingpolicy.hh"
#include "overlayflooding.hh"

CLICK_DECLS

OverlayFlooding::OverlayFlooding():
  _me(NULL),_ovl(NULL),
  _opportunistic(false),
  _responsable4parents(false)
{
  BRNElement::init();
}

OverlayFlooding::~OverlayFlooding()
{
}

void *
OverlayFlooding::cast(const char *name)
{
  if (strcmp(name, "OverlayFlooding") == 0)
    return dynamic_cast<OverlayFlooding *>(this);
  else if (strcmp(name, "FloodingPolicy") == 0)
         return dynamic_cast<FloodingPolicy *>(this);
       else
         return NULL;
}

int
OverlayFlooding::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
    "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
    "OVERLAY_STRUCTURE", cpkP+cpkM, cpElement, &_ovl,
    "OPPORTUNISTIC", cpkP, cpBool, &_opportunistic,
    "RESPONSABLE4PARENTS", cpkP, cpBool, &_responsable4parents,
    "DEBUG", cpkP, cpInteger, &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int
OverlayFlooding::initialize(ErrorHandler *)
{
  return 0;
}

void 
OverlayFlooding::init_broadcast(EtherAddress * , uint32_t, uint32_t *tx_data_size, uint8_t *,
                        Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
	//BRN_DEBUG("init_broadcast reached");
	do_forward(NULL,NULL,NULL,0,false,0,0,NULL,tx_data_size,NULL,unicast_dst,passiveack);
}

bool
OverlayFlooding::do_forward(EtherAddress *, EtherAddress * fwd, const EtherAddress *, uint32_t, bool is_known, uint32_t forward_count,
                           uint32_t, uint8_t *, uint32_t* /*tx_data_size*/, uint8_t *, Vector<EtherAddress> * unicast_dst, Vector<EtherAddress> * passiveack)
{
	//BRN_DEBUG("do_forward reached");
	if (is_known || (forward_count > 0)) return false;

	//TODO: _ovl->_pre ???
	if ( /*(_ovl->_pre) &&*/ (fwd!=0) && (!_opportunistic)) {
		Vector<EtherAddress> * parents=_ovl->getOwnParents();
		bool is_pre = (parents->empty());  //if no parents, forward
		
		if (!is_pre) {
			/*check whether packet is send by parent node*/
			for (Vector<EtherAddress>::iterator i=parents->begin();i!=parents->end();++i) {
				if ((*i)==(*fwd)) {
					is_pre=true;
					break;
				}
			}
		}

		if (!is_pre) return false; //don't forward packet which not send by parent nodes
	}

  Vector<EtherAddress> * children=_ovl->getOwnChildren();
  if (children->empty()) return false;

  //BRN_ERROR("------------------- %d -------------------------",children->size());

  for (Vector<EtherAddress>::iterator i=children->begin();i!=children->end();++i) {
	BRN_ERROR("Child: %s",i->unparse().c_str());
	unicast_dst->push_back(*i);
	passiveack->push_back(*i);
  }

  if ( _responsable4parents ) {
	Vector<EtherAddress> * parents=_ovl->getOwnParents();
	if (!parents->empty()) {
		for (Vector<EtherAddress>::iterator i=parents->begin();i!=parents->end();++i) {
			BRN_ERROR("Parent: %s",i->unparse().c_str());
			unicast_dst->push_back(*i);
			passiveack->push_back(*i);
		}
	}
  }

  return true;
}

int
OverlayFlooding::policy_id()
{
  return POLICY_ID_OVERLAY;
}

/*******************************************************************************************/
/************************************* H A N D L E R ***************************************/
/*******************************************************************************************/

void OverlayFlooding::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OverlayFlooding)
