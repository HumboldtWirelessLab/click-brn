/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * setsourceandoutputfordevice.{cc,hh}
 * A. Zubow
 */

#include <click/config.h>

//#include "brnprint.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/hashmap.hh>

#include "setsourceandoutputfordevice.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

SetSourceAndOutputForDevice::SetSourceAndOutputForDevice()
  : _debug(BrnLogger::DEFAULT)
{
}

SetSourceAndOutputForDevice::~SetSourceAndOutputForDevice()
{
}

int
SetSourceAndOutputForDevice::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ASSOCLIST", cpkP+cpkM, cpElement, /*"AssocList element",*/ &_stations,
      cpEnd) < 0)
    return -1;

  if (!_stations || !_stations->cast("AssocList")) 
    return errh->error("AssocList not specified");

  return 0;
}

int
SetSourceAndOutputForDevice::initialize(ErrorHandler *errh)
{
  _id = _stations->get_id();
  if (!_id || !_id->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  _link_table = _id->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  // get number of devices and device names from NodeIdentity and adjust output ports
  // there are as much output ports as devices in nodeidenty + 1 (for the local device)
  // THIS IS DONE STATICALLY AT THE MOMENT
  // PROBLEM IF INSERTING THE SAME ETHERNET ADDRESS TWO TIMES
  assert(NULL != _id->getMyWirelessAddress());
  if (NULL == _id->getMyWirelessAddress())
    return errh->error("not able to determine wireless address");
  
  if (!_ports.insert(*_id->getMyWirelessAddress(), 0))
  {
    BRN_WARN(" **** Could not insert ethernet addess %s to port 0.",
     *_id->getMyWirelessAddress()->unparse().c_str());
    return errh->error(" **** Could not insert ethernet addess %s to port 0.",
      *_id->getMyWirelessAddress()->unparse().c_str());
  }
  if (!_ports.insert(*_id->getMyVlan0Address(), 1))
  {
    BRN_DEBUG(" **** Could not insert ethernet addess %s to port 1.",
         _id->getMyVlan0Address()->unparse().c_str());
    return errh->error(" **** Could not insert ethernet addess %s to port 1.",
         _id->getMyVlan0Address()->unparse().c_str());
  }

  /*
  if (!_ports.insert(*_id->getMyVlan1Address(), 2))
  {
    click_chatter(" **** %s: Could not insert ethernet addess %s to port 2.", this->name().c_str(), _id->getMyVlan1Address()->unparse().c_str());
    return -1;
  }
  */

  if (_ports.size() + 1 != (unsigned int)noutputs())
    return errh->error("need %d output ports", _ports.size() + 1);

  return 0;
}

void
SetSourceAndOutputForDevice::uninitialize()
{
  //cleanup
}

/* Processes an incoming ethernet packet. */
void
SetSourceAndOutputForDevice::push(int, Packet *p_in)
{
  BRN_DEBUG(" * push()");

  click_ether *ether = (click_ether *) p_in->ether_header();

  if (!ether) 
  {
    BRN_ERROR(" SetSourceAndOutputForDevice: no ether header found.");
    p_in->kill();
    return;
  }

  // get src and dst mac 
  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  BRN_DEBUG(" The dst address %s (0x%x)and the src address %s (0x%x)",
      dst_addr.unparse().c_str(), &dst_addr, src_addr.unparse().c_str(), &src_addr);
  //click_chatter(" XXX The dst address %s (0x%x)and the src address %s (0x%x)", dst_addr2.unparse().c_str(), &dst_addr2, src_addr2.unparse().c_str(), &src_addr2);
  BRN_DEBUG(" **** _ports has %d entries.", _ports.size());

  if (dst_addr.is_broadcast())
    handle_broadcast(p_in);
  else
    handle_unicast(p_in);
}
  
void 
SetSourceAndOutputForDevice::handle_broadcast(Packet* p_in)
{ 
  // broadcast packet
  BRN_DEBUG(" * Handling broadcast packet.");

  click_ether *ether = (click_ether *) p_in->ether_header();
  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);

  // packet for wlan
  {
    Packet* p = p_in->clone();
    WritablePacket *p_wlan = (NULL == p) ? NULL : p->uniqueify();
    BRN_CHECK_EXPR_RETURN(NULL == p_wlan,
      ("Unable to copy packet."), p_in->kill(); return;);
  
    ether = (click_ether *) p_wlan->data();
    memcpy(ether->ether_shost, _id->getMyWirelessAddress()->data(), 6); 

    BRN_DEBUG(" **** WIRELESS Packet with dst %s and src %s is sent to port %d.",
      dst_addr.unparse().c_str(), _id->getMyWirelessAddress()->unparse().c_str(), 
      _ports.find(*_id->getMyWirelessAddress()));

    BRNPacketAnno::set_udevice_anno(p_wlan, _id->getWlan0DeviceName().c_str());
    p_wlan->set_ether_header(ether);
    output(_ports.find(*_id->getMyWirelessAddress())).push(p_wlan);
  }

  // packet for vlan0
  {
    Packet* p = p_in->clone();
    WritablePacket *p_vlan0 = (NULL == p) ? NULL : p->uniqueify();
    BRN_CHECK_EXPR_RETURN(NULL == p_vlan0,
      ("Unable to copy packet."), p_in->kill(); return;);
  
    ether = (click_ether *) p_vlan0->data();
    memcpy(ether->ether_shost, _id->getMyVlan0Address()->data(), 6); 

    BRN_DEBUG(" **** VLAN0 Packet with dst %s is sent to port %d.",
        dst_addr.unparse().c_str(), _ports.find(*_id->getMyVlan0Address()));

//    p_vlan0->set_udevice_anno(_id->getVlan0DeviceName().c_str());
    p_vlan0->set_ether_header(ether);
    output(_ports.find(*_id->getMyVlan0Address())).push(p_vlan0);
  }

  /*
  // packet for vlan1
  if (WritablePacket *p_vlan1 = p_in->uniqueify()) {
      ether = (click_ether *) p_vlan1->data();
      memcpy(ether->ether_shost, _id->getMyVlan1Address()->data(), 6); 
      p_vlan1->set_udevice_anno(_id->getVlan1DeviceName().c_str());
      output(_ports.find(*_id->getMyVlan1Address())).push(p_vlan1);
  }
  */

  // packet for local
  {
    // Do not clone the packet, use the incoming one.
    WritablePacket *p_local = p_in->uniqueify();
    BRN_CHECK_EXPR_RETURN(NULL == p_local,
      ("Unable to copy packet."), p_in->kill(); return;);

    ether = (click_ether *) p_local->data();

    BRN_DEBUG(" **** LOCAL Packet with dst %s is sent to port %d.",
        dst_addr.unparse().c_str(), _ports.size());

    BRNPacketAnno::set_udevice_anno(p_local,"local");
    p_local->set_ether_header(ether);
    output(_ports.size()).push(p_local);
  }
  // What is it good for to set the device annotation?
}

void
SetSourceAndOutputForDevice::handle_unicast(Packet* p_in)
{ 
  BRN_DEBUG(" * Handling unicast packet.");

  WritablePacket *p = p_in->uniqueify();
  if (!p) {
    BRN_ERROR("Killing packet.");
    p_in->kill();
    return;
  }
  click_ether *ether = (click_ether *) p_in->ether_header();
  EtherAddress dst_addr(ether->ether_dhost);
  EtherAddress src_addr(ether->ether_shost);
    
  // packet to me
  if (_id->isIdentical(&dst_addr)) 
  {
    BRN_DEBUG(" * Packet (%s) for me (%s,%s) - push to port %u", dst_addr.unparse().c_str(),
      _id->getMyWirelessAddress()->unparse().c_str(), _id->getMyVlan0Address()->unparse().c_str(),
      _ports.size());

    // TODO do I need to change the src address ??
    memcpy(ether->ether_shost, _id->getMyWirelessAddress()->data(), 6);
    output(_ports.size()).push(p);
    //output().push(p);
  }
  // packet to station
  else if (_stations->is_associated(dst_addr)) 
  {
    BRN_DEBUG(" * Packet for associated client");

    String dev(_stations->get_device_name(dst_addr));
    EtherAddress* addr_ptr = _id->getEthernetAddress(dev);
    if (!addr_ptr) {
      BRN_ERROR("Killing packet.");
      p_in->kill();
      return;
    }

    // set source address for the device
    memcpy(ether->ether_shost, addr_ptr->data(), 6);
    output(_ports.find(*addr_ptr)).push(p);
  }
  // packet to mesh neighbor?
  else 
  {
    BRN_DEBUG(" * Packet for other meshnodes");
    // try first to reach dst_addr via ethernet
    if (_link_table->get_link_metric(*_id->getMyVlan0Address(), dst_addr) < BRN_DSR_INVALID_ROUTE_METRIC) {
      ether = (click_ether *) p->data();
      memcpy(ether->ether_shost, _id->getMyVlan0Address()->data(), 6); 
      output(_ports.find(*_id->getMyVlan0Address())).push(p);
      return;
    }
    else if (_link_table->get_link_metric(*_id->getMyVlan1Address(), dst_addr) < BRN_DSR_INVALID_ROUTE_METRIC) {
      ether = (click_ether *) p->data();
      memcpy(ether->ether_shost, _id->getMyVlan1Address()->data(), 6); 
      output(_ports.find(*_id->getMyVlan1Address())).push(p);
      return;
    }
    // and then via wireless
    else if (_link_table->get_link_metric(*_id->getMyWirelessAddress(), dst_addr) < BRN_DSR_INVALID_ROUTE_METRIC) {
      ether = (click_ether *) p->data();
      memcpy(ether->ether_shost, _id->getMyWirelessAddress()->data(), 6); 
      output(_ports.find(*_id->getMyWirelessAddress())).push(p);
      return;
    } else {
      // maybe ethernet client
	  BRN_DEBUG(" * Packet for ethernet client");

      EtherAddress* addr_ptr = _id->getMyVlan0Address();
      if (!addr_ptr) {
      	BRN_ERROR("Killing packet.");
      	p_in->kill();
      	return;
      }
      
      // set source address for the device
      //memcpy(ether->ether_shost, addr_ptr->data(), 6);
      output(_ports.find(*addr_ptr)).push(p);
      return;
    }
    
    BRN_WARN("Killing packet.");
    p->kill();
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  SetSourceAndOutputForDevice *ds = (SetSourceAndOutputForDevice *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  SetSourceAndOutputForDevice *ds = (SetSourceAndOutputForDevice *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
SetSourceAndOutputForDevice::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(SetSourceAndOutputForDevice)
#include <click/bighashmap.cc>
#if EXPLICIT_TEMPLATE_INSTANCES
#endif
CLICK_ENDDECLS
