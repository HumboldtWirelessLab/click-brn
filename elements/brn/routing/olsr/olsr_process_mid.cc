// TED 260404: Created

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include "olsr_process_mid.hh"
#include "olsr_packethandle.hh"
#include "click_olsr.hh"

CLICK_DECLS

OLSRProcessMID::OLSRProcessMID()
 : _interfaceInfo(NULL),_routingTable(NULL)
{
}


OLSRProcessMID::~OLSRProcessMID()
{
}


int
OLSRProcessMID::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "InterfaceInfoBase Element", cpkP, cpElement,&_interfaceInfo,
      "Routing Table Element", cpkP, cpElement,&_routingTable,
		  cpEnd) < 0)
    return -1;
  return 0;
}

//Output ports:
//0 - Consider for forward
//1 - Discard

void
OLSRProcessMID::push(int, Packet *packet){

  bool interface_changed = false;
  msg_hdr_info msg_info;
  int mid_msg_offset, bytes_left;
  struct timeval now;

  now = Timestamp::now().timeval();
  msg_info = OLSRPacketHandle::get_msg_hdr_info(packet, 0);

  mid_msg_offset = sizeof(olsr_msg_hdr);
  bytes_left = msg_info.msg_size - sizeof(olsr_msg_hdr);

  do{
    const in_addr *address = reinterpret_cast<const in_addr *>( (packet->data() + mid_msg_offset));
    IPAddress interface_address = IPAddress(*address);
    interface_data* data = _interfaceInfo->find_interface(interface_address);
    
    if ( data == 0 ){
      _interfaceInfo->add_interface(interface_address, msg_info.originator_address, (Timestamp(now) + Timestamp(msg_info.validity_time)).timeval());
//      click_chatter("adding new interface: %s\n", interface_address.unparse().cc()); 
      interface_changed = true;
    }
    else{
//      click_chatter("found interface %s, updating\n", interface_address.unparse().cc());
      _interfaceInfo->update_interface(interface_address, (Timestamp(now) + Timestamp(msg_info.validity_time)).timeval());
    }
    bytes_left -= sizeof(in_addr);
    mid_msg_offset += sizeof(in_addr);
  }while ( bytes_left >= (int) sizeof(in_addr) );

  //_interfaceInfo->print_interfaces();
  if (interface_changed)
  { //_interfaceInfo->print_interfaces();
    _routingTable->compute_routing_table();
  }
  output(0).push(packet);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRProcessMID);

