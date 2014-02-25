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

#ifndef OVERLAY_STRUCTURE_HH
#define OVERLAY_STRUCTURE_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnelement.hh"
#include <elements/brn/routing/identity/brn2_nodeidentity.hh>

CLICK_DECLS

class OverlayStructure : public BRNElement {

   

 public:
  OverlayStructure();
  ~OverlayStructure();

  const char *class_name() const  { return "OverlayStructure"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  int initialize(ErrorHandler *);

  void add_handlers();

  Vector <EtherAddress> parents; // Parents of the node
  Vector <EtherAddress> children; // Children of the node
  HashMap <EtherAddress, Vector <EtherAddress> > n_parents; // Parents of neighbouring nodes
  HashMap <EtherAddress, Vector <EtherAddress> > n_children; // Children of neighbouring nodes
  
  void addOwnParent (EtherAddress* add); //add - EtherAdress of new parent
  void addOwnChild (EtherAddress* add); //add - EtherAdress of new Child
  void addParent (EtherAddress* node, EtherAddress* add);  //add - EtherAdress of new parent, node - EtherAdress of node to get new parent
  void addChild (EtherAddress* node, EtherAddress* add);  //add - EtherAdress of new child, node - EtherAdress of node to get new child

  Vector <EtherAddress>* getOwnParents(); //returns parents of the node
  Vector <EtherAddress>* getOwnChildren(); //returns children of the node
  Vector <EtherAddress>* getParents(EtherAddress* node); //returns parents of the node specified in node; return 0 if node is not known
  Vector <EtherAddress>* getChildren(EtherAddress* node); //returns children of the node specified in node; return 0 if node is not known

  String printOwnParents();
  String printOwnChildren();
  String printParents(const EtherAddress* add, const Vector<EtherAddress> *par);
  String printChildren(const EtherAddress* add, const Vector<EtherAddress> *par);
  String printAllParents();
  String printAllChildren();

 private:

  BRN2NodeIdentity *_me;

};

CLICK_ENDDECLS
#endif
