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
 * nblist.{cc,hh} -- a list of information about neighbor nodes (brn-nodes)
 * A. Zubow
 */

#include <click/config.h>
#include "elements/brn/common.hh"
#include "nblist.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/standard/brnpacketanno.hh"
CLICK_DECLS

NeighborList::NeighborList() :
  _debug(BrnLogger::DEFAULT)
{
  _nb_list = new NBMap();
}

NeighborList::~NeighborList()
{
  BRN_DEBUG(" * current nb list: %s", printNeighbors().c_str());

  delete _nb_list;
}

int
NeighborList::configure(Vector<String> &, ErrorHandler* )
{
  return 0;
}

int
NeighborList::initialize(ErrorHandler *)
{
  return 0;
}

/* should be an annotated brn packet */
Packet *
NeighborList::simple_action(Packet *p_in)
{
  //const click_ether *ether = (const click_ether *)p_in->data();
  const click_ether *ether = (click_ether *)p_in->ether_header();

  if (ether) {
    String device(BRNPacketAnno::udevice_anno(p_in));
    EtherAddress nb_node(ether->ether_shost);

    //  BRN_DEBUG(" * NeighborList %s ::setting %s -> %s\n", id().c_str(), nb_node.unparse().c_str(), device.c_str());
    // dirty HACK, TODO
#ifdef CLICK_NS  //TODO: should be remove, also the annotation-stuff for device (whats that kind of routing ??
    if (device == "")
      device = String("eth0");
#else
    if (device == "")
      device = String("ath0");
#endif

    insert(nb_node, device);
  }

  return p_in;
}

/* checks if a node with the given ethernet address is associated with us */
bool
NeighborList::isContained(EtherAddress *v)
{
  return (NULL != _nb_list->findp(*v));
}

NeighborList::NeighborInfo *
NeighborList::getEntry(EtherAddress *v)
{
  return _nb_list->findp(*v);
}

String
NeighborList::printNeighbors()
{
  StringAccum sa;
  for (NBMap::iterator i = _nb_list->begin(); i.live(); i++) {
    NeighborInfo &nb_info = i.value();
    sa << " * nb: " << nb_info._eth.unparse() << " via device " << nb_info._dev_name << " reachable.\n";
  }
  return sa.take_string();
}

int
NeighborList::insert(EtherAddress eth, String dev_name) 
{
  if (!(eth && dev_name)) {
    BRN_WARN("* You idiot, you tried to insert %s, %s", eth.unparse().c_str(), dev_name.c_str());
    return -1;
  }

  NeighborInfo *nb_info = _nb_list->findp(eth);
  if (!nb_info) {
    _nb_list->insert(eth, NeighborInfo(eth));
    BRN_DEBUG(" * new entry added2: %s, %s", eth.unparse().c_str(), dev_name.c_str());
    nb_info = _nb_list->findp(eth);
  }
  nb_info->_dev_name = dev_name;

  return 0;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  NeighborList *nl = (NeighborList *)e;
  return String(nl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  NeighborList *nl = (NeighborList *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  nl->_debug = debug;
  return 0;
}

/*
 * Adds a new entry into _nb_list.
 * Format: insert $INTERFACE $ETHERNET_ADDR
 */
static int 
static_insert(const String &arg, Element *e, void *, ErrorHandler *errh)
{

  NeighborList *nb = (NeighborList *)e;

  if (nb->_debug)
    click_chatter(" * receiving arg: %s", arg.c_str());

  Vector<String> args;
  EtherAddress eth;
  String dev_name;
  cp_spacevec(arg, args);

  if (args.size() % 2 != 0) {
    return errh->error("Must have mod two arguments: currently has %d: %s",
        args.size(), args[0].c_str());
  }

  for (int x = 0; x < args.size(); x += 2) {
    if (!cp_ethernet_address(args[x], &eth)) {
      return errh->error("Couldn't read EthernetAddress out of arg");
    }

    if (!cp_string(args[x + 1], &dev_name)) {
      return errh->error("Couldn't read device name");
    }

    if (nb->insert(eth, dev_name) < 0) {
      return -1;
    }
  }
  return 0;
}

void
NeighborList::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);

  add_write_handler("insert", static_insert, 0);
  add_write_handler("debug", write_debug_param, 0);
}

#include <click/bighashmap.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(NeighborList)
