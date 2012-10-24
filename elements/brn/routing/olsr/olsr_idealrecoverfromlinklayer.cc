
#include <click/config.h>
#include <click/confparse.hh>
#include "olsr_idealrecoverfromlinklayer.hh"
#include "click_olsr.hh"
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

CLICK_DECLS


OLSRIdealRecoverFromLinkLayer::OLSRIdealRecoverFromLinkLayer()
{
}


OLSRIdealRecoverFromLinkLayer::~OLSRIdealRecoverFromLinkLayer()
{
}


int
OLSRIdealRecoverFromLinkLayer::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh,
      "NeighborInfoBase Element", cpkP, cpElement, &_neighborInfoBase,
      "LinkInfoBase Element", cpkP, cpElement, &_linkInfoBase,
      "InterfaceInfoBase Element", cpkP, cpElement, &_interfaceInfoBase,
      "TCGenerator Element", cpkP, cpElement, &_tcGenerator,
      "RoutingTable Element",cpkP, cpElement,  &_routingTable,
      "Nodes main IP address", cpkP, cpIPAddress,&_myMainIP,
	                cpEnd) < 0)
		return -1;
	return 0;
}


int
OLSRIdealRecoverFromLinkLayer::initialize(ErrorHandler *)
{
	return 0;
}

void
OLSRIdealRecoverFromLinkLayer::notify(const IPAddress &next_hop_IP)
{
	timeval now;
  now = Timestamp::now().timeval();

	/// @TODO what to do with timeval2double
	/// click_chatter("%f | %s | %s | ideal recover: next hop: %s", timeval2double(now), _myMainIP.unparse().c_str(), __FUNCTION__, next_hop_IP.s().c_str());
	// remove the matching link from the link info base
	IPAddress next_hop_main_IP = _interfaceInfoBase->get_main_address(next_hop_IP);
	_linkInfoBase->remove_link(_myMainIP, next_hop_IP);
	// remove the matching neighbor from the neighbor info base if there are no more links
	bool other_interfaces_left = false;
	bool mpr_selector_removed = true;
	for (OLSRLinkInfoBase::LinkSet::iterator iter = _linkInfoBase->get_link_set()->begin(); iter != _linkInfoBase->get_link_set()->end(); iter++)
	{
		link_data *data = (link_data *)iter.value();
		if (_interfaceInfoBase->get_main_address(data->L_neigh_iface_addr) == next_hop_main_IP)
		{
			other_interfaces_left = true;
			break;

		}
	}
	if (!other_interfaces_left)
	{
		_neighborInfoBase->remove_neighbor(next_hop_main_IP);
		if (_neighborInfoBase->find_mpr_selector (next_hop_main_IP))
		{
			_neighborInfoBase->remove_mpr_selector(next_hop_main_IP);
			mpr_selector_removed=true;
		}
	}
	// make sure that the MPRs and Route Table are updated
	if (mpr_selector_removed) _tcGenerator->notify_mpr_selector_changed();
	_neighborInfoBase->compute_mprset();
	_routingTable->compute_routing_table();

}

int
OLSRIdealRecoverFromLinkLayer::notify_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	OLSRIdealRecoverFromLinkLayer* me = (OLSRIdealRecoverFromLinkLayer *) e;
	IPAddress next_hop_ip;
  int res = cp_va_kparse( conf, me, errh, "Next Hop IP",  cpkP, cpIPAddress, &next_hop_ip, cpEnd );
	if ( res < 0 )
		return res;
	me->notify(next_hop_ip);
	return res;
}

void
OLSRIdealRecoverFromLinkLayer::add_handlers()
{
	add_write_handler("notify", &notify_handler, (void *)0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRIdealRecoverFromLinkLayer);

