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

#ifndef NBLISTELEMENT_HH
#define NBLISTELEMENT_HH

#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/element.hh>

CLICK_DECLS
/*
 * =c
 * NeighborList()
 * =s a list of information about neighbor nodes (brn-nodes)
 * stores the ethernet address of a neighbor node and a interface to reach it (e.g. wlan0).
 * =d
 * 
 * TODO currently not entries will removed from the the list, implement a way
 * to remove them.
 */
class NeighborList : public Element {

 public:
  //
  //inner classes
  //
  class NeighborInfo { public:
    EtherAddress _eth;
    String _dev_name;
    //struct timeval _when;
    NeighborInfo() { 
//      memset(this, 0, sizeof(*this));
    }

    NeighborInfo(EtherAddress eth) { 
//      memset(this, 0, sizeof(*this));
      _eth = eth;
    }
  };


  typedef HashMap<EtherAddress, NeighborInfo> NBMap;
  //
  //member
  //
  int _debug;
  NBMap *_nb_list;

  //
  //methods
  //
  NeighborList();
  ~NeighborList();

  const char *class_name() const	{ return "NeighborList"; }
  const char *port_count() const	{ return "1/1"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  Packet *simple_action(Packet *);
  int initialize(ErrorHandler *);
  void add_handlers();

  String printNeighbors();

  bool isContained(EtherAddress *v);
  NeighborInfo *getEntry(EtherAddress *v);
  int insert(EtherAddress eth, String dev_name);
};

CLICK_ENDDECLS
#endif
