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
 * brnds.{cc,hh} -- forwards brn packets to the right device
 * A. Zubow
 */

#include <click/config.h>

#include "brnds.hh"
#include "elements/brn/brnprotocol/brnprint.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

BRNDS::BRNDS()
  :_debug(BrnLogger::DEFAULT),
   _me(NULL),
   _nb_lst(NULL),
   _link_table(NULL)
{
}

BRNDS::~BRNDS()
{
}

int
BRNDS::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, /*"NodeIdentity",*/ &_me,
      "NEIGHBORLIST", cpkP+cpkM, cpElement, /*"NeighborList",*/ &_nb_lst,
      cpEnd) < 0)
    return -1;

  if (!_me || !_me->cast("NodeIdentity")) 
    return errh->error("NodeIdentity not specified");

  if (!_nb_lst || !_nb_lst->cast("NeighborList")) 
    return errh->error("NeighborList not specified");

  return 0;
}

int
BRNDS::initialize(ErrorHandler *errh)
{
  _link_table = _me->get_link_table();
  if (!_link_table || !_link_table->cast("BrnLinkTable")) 
    return errh->error("BRNLinkTable not specified");

  return 0;
}

void
BRNDS::uninitialize()
{
  //cleanup
}

/* Processes an incoming (brn-)packet. */
void
BRNDS::push(int port, Packet *p_in)
{
  UNREFERENCED_PARAMETER(port);

  BRN_DEBUG("push()");

  //append ethernet header
  click_brn_dsr *brn_dsr =
        (click_brn_dsr *)(p_in->data() + sizeof(click_brn));
  EtherAddress dst_anno = BRNPacketAnno::dst_ether_anno(p_in);
  String device(BRNPacketAnno::udevice_anno(p_in));

  BRN_DEBUG("* dst ethernet anno: %s", dst_anno.unparse().c_str());

  //estimate packet type
  if (brn_dsr->dsr_type == BRN_DSR_RREQ) { // broadcast packet

    if (WritablePacket *p_wlan0 = p_in->push(14)) {
      BRN_DEBUG("* handle rreq (dev_anno=%s).\n", device.c_str());
      EtherAddress bcast((const unsigned char *)"\xff\xff\xff\xff\xff\xff");

      // packet for wlan0
      click_ether *ether = (click_ether *) p_wlan0->data();
      memcpy(ether->ether_dhost, bcast.data(), 6); // rreq is a broadcast
      ether->ether_type = htons(ETHERTYPE_BRN);
      memcpy(ether->ether_shost, _me->getMyWirelessAddress()->data(), 6); 

      // packet for vlan0
      WritablePacket *p_vlan0 = Packet::make(p_wlan0->length());
      memcpy(p_vlan0->data(), p_wlan0->data(), p_wlan0->length());
      ether = (click_ether *) p_vlan0->data();
      memcpy(ether->ether_shost, _me->getMyVlan0Address()->data(), 6); 

      // packet for vlan1
      WritablePacket *p_vlan1 = Packet::make(p_wlan0->length());
      memcpy(p_vlan1->data(), p_wlan0->data(), p_wlan0->length());
      ether = (click_ether *) p_vlan1->data();
      memcpy(ether->ether_shost, _me->getMyVlan0Address()->data(), 6); 

      //check whether I need to append myself into rreq hop list; this could be possible
      //due to the change of the used medium
      if (device == _me->getWlan0DeviceName()) { //wlan0
        BRN_DEBUG("* packet received over wlan0 should be forwarded over vlan0; append my wlan address to hop route.");
        // append ethernet address of wlan0 to packets which should be transmitted over the vlans
        append_hop_to_rreq(p_vlan0, _me->getMyWirelessAddress());
        append_hop_to_rreq(p_vlan1, _me->getMyWirelessAddress());
      } else if ( (device == _me->getVlan0DeviceName()) || (device == _me->getVlan1DeviceName()) ) { //vlan0/1
        BRN_DEBUG("* packet received over vlan0/1 should be forwarded over wlan0; append my vlan0/1 address to hop route.");
        // append ethernet address of wlan0 to packets which should be transmitted over the vlans
        append_hop_to_rreq(p_wlan0, _me->getMyVlan0Address()); //_me->getMyVlan1Address() is the same
      }

      //set device anno
      BRNPacketAnno::set_udevice_anno(p_wlan0,_me->getWlan0DeviceName().c_str());
      BRNPacketAnno::set_udevice_anno(p_vlan0,_me->getVlan0DeviceName().c_str());
      BRNPacketAnno::set_udevice_anno(p_vlan1,_me->getVlan1DeviceName().c_str());

      //output packets on all available devices
      output(0).push(p_wlan0);
      output(1).push(p_vlan0);
      output(2).push(p_vlan1);
    }
  // unicast packets
  } else if ( (brn_dsr->dsr_type == BRN_DSR_RREP) 
    || (brn_dsr->dsr_type == BRN_DSR_SRC) 
    || (brn_dsr->dsr_type == BRN_DSR_RERR) ) 
  {
    BRN_DEBUG("* ds() handle rrep or src or rerr.");

    NeighborList::NeighborInfo *nb_info;

    if ( (nb_info = _nb_lst->getEntry(&dst_anno)) ) { // neighbor found
      BRN_DEBUG("* using device= %s", nb_info->_dev_name.c_str());

      if (WritablePacket *q = p_in->push(14)) {
        BRN_DEBUG("* handle unicast packet.");

        // packet for wlan0
        click_ether *ether = (click_ether *) q->data();
        memcpy(ether->ether_dhost, dst_anno.data(), 6); // using dest from anno field
        ether->ether_type = htons(ETHERTYPE_BRN);

        if (nb_info->_dev_name == _me->getWlan0DeviceName()) {
          memcpy(ether->ether_shost, _me->getMyWirelessAddress()->data(), 6); 
          output(0).push(q);
        } else if (nb_info->_dev_name == _me->getVlan0DeviceName()) {
          memcpy(ether->ether_shost, _me->getMyVlan0Address()->data(), 6); 
          output(1).push(q);
        } else if (nb_info->_dev_name == _me->getVlan1DeviceName()) {
          memcpy(ether->ether_shost, _me->getMyVlan0Address()->data(), 6); 
          output(2).push(q);
        } else {
          BRN_ERROR("* Unknown device: %s", nb_info->_dev_name.c_str());
        }
      } else {
        BRN_FATAL("Could not allocate memory !!!");
      }
    } 
    else 
    {
      BRN_WARN("* Warning: Neighbor with ether address %s unknown.", dst_anno.unparse().c_str());
      BRN_DEBUG("* LinkTable: %s", _link_table->print_links().c_str());
      BRN_DEBUG("* Neighbors: %s", _nb_lst->printNeighbors().c_str());

      checked_output_push(3, p_in);
    }
  } else {
    BRN_WARN("* unhandled packet: type = %d\n", brn_dsr->dsr_type);
    p_in->kill();
  }
}

/* manipulating brn packet */
void
BRNDS::append_hop_to_rreq(Packet *p, EtherAddress *extra_hop)
{
  // output on eth1: add hop
  click_brn_dsr *dsr = (click_brn_dsr *)(p->data() + sizeof(click_ether) + sizeof(click_brn));

  BRN_DEBUG("* append_hop_to_rreq(): hops before %d\n", dsr->dsr_hop_count);

//  if ( (brn->dst_port == BRN_PORT_DSR) &&
//    (brn->src_port == BRN_PORT_DSR) && (dsr->dsr_type == BRN_DSR_RREQ) ) {
    // packet is a brn route request
    BRN_DEBUG("* append_hop_to_rreq(): need to add the virtual (in-memory) link to hop list in rreq.\n");

    memcpy(dsr->addr[dsr->dsr_hop_count].hw.data, (uint8_t *)extra_hop->data(), 6 * sizeof(uint8_t));
    dsr->addr[dsr->dsr_hop_count].metric = htons(BRN_DSR_MEMORY_MEDIUM_METRIC);
    dsr->dsr_hop_count++;

//  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRNDS *ds = (BRNDS *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRNDS *ds = (BRNDS *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
BRNDS::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

EXPORT_ELEMENT(BRNDS)

CLICK_ENDDECLS
