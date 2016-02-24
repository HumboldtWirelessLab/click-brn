//TED 150404: created

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/ipaddress.hh>
#include <click/packet_anno.hh>
#include "click_olsr.hh"
#include "olsr_neighbor_infobase.hh"
#include "olsr_process_hello.hh"
#include "olsr_packethandle.hh"
#include "clicknet/ether.h"

CLICK_DECLS

OLSRProcessHello::OLSRProcessHello()
: _linkInfo(NULL),_neighborInfo(NULL),_interfaceInfo(NULL),_routingTable(NULL),_tcGenerator(NULL),_localIfInfoBase(NULL)
{
}

OLSRProcessHello::~OLSRProcessHello()
{
}


int
OLSRProcessHello::configure(Vector<String> &conf, ErrorHandler *errh)
{
	int neighbor_hold_time;

	if (cp_va_kparse(conf, this, errh,
      "Neihbor Hold time",cpkP,cpInteger,&neighbor_hold_time,
                 "LinkInfoBase Element",cpkP, cpElement, &_linkInfo,
                 "NeighborInfoBase Element", cpkP, cpElement,&_neighborInfo,
                "InterfaceInfoBase Element", cpkP, cpElement,&_interfaceInfo,
                 "Routing Table Element", cpkP, cpElement,&_routingTable,
                 "TC generator element", cpkP, cpElement,&_tcGenerator,
                 "localIfInfoBase Element", cpkP, cpElement,&_localIfInfoBase,
                 "Main IPAddress of node",cpkP, cpIPAddress,&_myMainIp,  cpEnd) < 0)
		return -1;
	_neighbor_hold_time_tv=Timestamp((int) (neighbor_hold_time / 1000),(neighbor_hold_time % 1000)).timeval();
	return 0;
}

void
OLSRProcessHello::push(int, Packet *packet)
{

	msg_hdr_info msg_info;
	hello_hdr_info hello_info;
	link_hdr_info link_info;
	link_data *link_tuple;
	neighbor_data *neighbor_tuple;

	bool update_twohop = false;
	bool twohop_deleted = false;
	bool new_twohop_added = false;
	bool new_neighbor_added = false;
	bool mpr_selector_added = false;
	struct timeval now;
	IPAddress neighbor_main_address, originator_address, source_address;
  now = Timestamp::now().timeval();
	msg_info = OLSRPacketHandle::get_msg_hdr_info(packet, 0);

	//click_chatter ("Process HELLO: validity time %d %d",  msg_info.validity_time.tv_sec,msg_info.validity_time.tv_usec);
	originator_address = msg_info.originator_address;
	//dst_ip_anno must be set, must be source address of ippacket
	source_address = packet->dst_ip_anno();

	int paint=static_cast<int>(PAINT_ANNO(packet));//packets get marked with paint 0..N depending on Interface they arrive on
	IPAddress receiving_If_IP=_localIfInfoBase->get_iface_addr(paint); //gets IP of Interface N
	//7.1.1 - 1
	link_tuple = _linkInfo->find_link(receiving_If_IP, source_address);

	if (link_tuple == NULL)
	{
		link_tuple = _linkInfo->add_link(receiving_If_IP, source_address, (Timestamp(now) + Timestamp(msg_info.validity_time)).timeval());
		link_tuple->L_SYM_time = (Timestamp(now) - Timestamp(1,0)).timeval();
    link_tuple->L_ASYM_time = (Timestamp(now) + msg_info.validity_time).timeval();
		link_tuple->_main_addr = originator_address;
		click_chatter("%s adding link to %s\n", receiving_If_IP.unparse().c_str(), source_address.unparse().c_str());
	}
	else
	{
    link_tuple->L_ASYM_time = (Timestamp(now) + msg_info.validity_time).timeval();
		if ( _interfaceInfo->get_main_address(link_tuple->L_neigh_iface_addr) == originator_address)
		{ // From RFC 8.2.1
      if (Timestamp(link_tuple->L_SYM_time) >= Timestamp(now))
				update_twohop = true;
		}
	}
	hello_info = OLSRPacketHandle::get_hello_hdr_info(packet, sizeof(olsr_msg_hdr));
	//from RFC 8.1
	neighbor_main_address = originator_address;
	neighbor_tuple = _neighborInfo->find_neighbor(neighbor_main_address);
	if (neighbor_tuple == NULL)
	{
		neighbor_tuple = _neighborInfo->add_neighbor(neighbor_main_address);
		neighbor_tuple->N_status = OLSR_NOT_NEIGH;
	}
	neighbor_tuple->N_willingness = hello_info.willingness;
	//end 8.1
	// end 7.1.1
	int link_msg_bytes_left = msg_info.msg_size - sizeof(olsr_msg_hdr) - sizeof(olsr_hello_hdr);
	int link_msg_offset = sizeof(olsr_msg_hdr) + sizeof(olsr_hello_hdr);
	int address_offset = link_msg_offset + sizeof(olsr_link_hdr);
	if (link_msg_bytes_left > 0)
	{ //there are advertised links in the hello-message
		do
		{
			link_info = OLSRPacketHandle::get_link_hdr_info(packet, link_msg_offset);
			int interface_address_bytes_left = link_info.link_msg_size - sizeof(olsr_link_hdr);
			do
			{
				const in_addr *address = reinterpret_cast<const in_addr *>( (packet->data() + address_offset));
				IPAddress neighbor_address = IPAddress(*address);

				//from RFC 7.1.1 - 2
				if (neighbor_address == receiving_If_IP)
				{
					if (link_info.link_type == OLSR_LOST_LINK)
					{
						link_tuple->L_SYM_time = (Timestamp(now) - Timestamp(1,0)).timeval();  // == expired
					}
					else if (link_info.link_type == OLSR_SYM_LINK || link_info.link_type == OLSR_ASYM_LINK)
					{
						link_tuple->L_SYM_time = (Timestamp(now) + Timestamp(msg_info.validity_time)).timeval();
            link_tuple->L_time = (Timestamp(link_tuple->L_SYM_time) + Timestamp(_neighbor_hold_time_tv)).timeval();
					}

          if (Timestamp(link_tuple->L_time) < Timestamp(link_tuple->L_ASYM_time))
					{
						link_tuple->L_time = link_tuple->L_ASYM_time;
					}

					//from RFC 8.1
          if ( Timestamp(link_tuple->L_SYM_time) >= Timestamp(now) )
					{
						if (neighbor_tuple->N_status != OLSR_SYM_NEIGH)
						{ //RFC 8.5
							new_neighbor_added = true;
						}
						neighbor_tuple->N_status = OLSR_SYM_NEIGH;
					}
					else
					{
						neighbor_tuple->N_status = OLSR_NOT_NEIGH;
					}
					// end 7.1.1 - 2
				}

				//from RFC 8.2.1
				if(update_twohop && (link_info.neigh_type==OLSR_SYM_NEIGH||link_info.neigh_type==OLSR_MPR_NEIGH))
				{
					if ( neighbor_address != _myMainIp )
					{
						IPAddress main_neighbor_address = _interfaceInfo->get_main_address(neighbor_address);
						if (_neighborInfo->find_twohop_neighbor(originator_address, main_neighbor_address) == 0)
							new_twohop_added = true;
            _neighborInfo->add_twohop_neighbor(originator_address, main_neighbor_address, (Timestamp(now)+Timestamp(msg_info.validity_time)).timeval());
					}
				}
				else if (update_twohop && link_info.neigh_type == OLSR_NOT_NEIGH)
				{
					IPAddress main_neighbor_address = _interfaceInfo->get_main_address(neighbor_address);
					_neighborInfo->remove_twohop_neighbor(originator_address, main_neighbor_address);
					twohop_deleted = true; //RFC 8.5
				}	// end 8.2.1

				// from RFC 8.4.1
				//click_chatter ("main address of neighbor: %s, my Main IP %s\n",_interfaceInfo->get_main_address(neighbor_address).unparse().c_str(),_myMainIp.unparse().c_str());
				if ( _interfaceInfo->get_main_address(neighbor_address) == _myMainIp )
				{
					if (link_info.neigh_type == OLSR_MPR_NEIGH )
					{
						mpr_selector_data *mpr_selector = _neighborInfo->find_mpr_selector(originator_address);
						if (mpr_selector == 0)
						{
              mpr_selector = _neighborInfo->add_mpr_selector(originator_address, (Timestamp(now) + Timestamp(msg_info.validity_time)).timeval());
							mpr_selector_added = true;
						}
						else
						{
							mpr_selector->MS_time = (Timestamp(now) + Timestamp(msg_info.validity_time)).timeval();
						}
					}
				}//end 8.4.1

				interface_address_bytes_left -= sizeof(in_addr);
				address_offset += sizeof(in_addr);
			}
			while ( interface_address_bytes_left >= (int) sizeof(in_addr) );

			link_msg_bytes_left -= link_info.link_msg_size;
			link_msg_offset += link_info.link_msg_size;
			address_offset += sizeof(olsr_link_hdr);
		}
		while (  link_msg_bytes_left >= (int) ( sizeof(olsr_link_hdr) + sizeof(in_addr) )  );
	}

	//_neighborInfo->print_mpr_selector_set();
	if ( mpr_selector_added )
		_tcGenerator->notify_mpr_selector_changed();  //this includes incrementing ansn; if activated an additional tc message is sent;
	// in a strictly RFC interpretation this should only be done if change is based on link failure
	if (twohop_deleted || new_twohop_added || new_neighbor_added)
	{
		_neighborInfo->compute_mprset();
		_routingTable->compute_routing_table();
	}
	output(0).push(packet);
}


/// == mvhaen ====================================================================================================
void
OLSRProcessHello::set_neighbor_hold_time_tv(int neighbor_hold_time)
{
	_neighbor_hold_time_tv=Timestamp((int) (neighbor_hold_time / 1000),(neighbor_hold_time % 1000)).timeval();
	click_chatter ("_neighbor_hold_time_tv = %d %d\n", _neighbor_hold_time_tv.tv_sec,  _neighbor_hold_time_tv.tv_usec);
}

int
OLSRProcessHello::set_neighbor_hold_time_tv_handler(const String &conf, Element *e, void *, ErrorHandler * errh)
{
	OLSRProcessHello* me = reinterpret_cast<OLSRProcessHello *>( e);
	int new_nbr_hold_time;
  int res = cp_va_kparse( conf, me, errh, "Neighbor Hold time", cpkP, cpInteger, &new_nbr_hold_time, cpEnd );
	if ( res < 0 )
		return res;
	me->set_neighbor_hold_time_tv(new_nbr_hold_time);
	return res;
}

void
OLSRProcessHello::add_handlers()
{
	add_write_handler("set_neighbor_hold_time_tv", set_neighbor_hold_time_tv_handler, (void *)0);
}
/// == !mvhaen ===================================================================================================

CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRProcessHello)

