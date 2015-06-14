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
 * topology_detection.{cc,hh} -- encapsulates packet in Ethernet header
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/userutils.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "overlay_structure.hh"

CLICK_DECLS

OverlayStructure::OverlayStructure() 
{
  BRNElement::init();
}

OverlayStructure::~OverlayStructure()
{
}

int
OverlayStructure::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String overlayfilepath = "";

  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "OVERLAYFILE", cpkP, cpString, &overlayfilepath,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  _pre = false;

  //click_chatter("Filename(%d): %s",overlayfilepath.length(), overlayfilepath.c_str());
  if ( overlayfilepath.length() > 0 )
    read_overlay_from_file(overlayfilepath);

  return 0;
}

int
OverlayStructure::initialize(ErrorHandler *)
{
  return 0;
}

void
OverlayStructure::read_overlay_from_file(String path) {
  String _data = file_string(path);
  Vector<String> _data_vec;
  cp_spacevec(_data, _data_vec);

  EtherAddress act;
  EtherAddress last = EtherAddress::make_broadcast();
  EtherAddress first = EtherAddress::make_broadcast();
  int i=0;

  //click_chatter("Size: %d",_data_vec.size());

  cp_ethernet_address(_data_vec[_data_vec.size()-1],&act); //read last int. if it is broadcast than its a circle file

  if ( !act.is_broadcast() ) { //no broadcast -> no circle

	while (i < _data_vec.size()) {
		cp_ethernet_address(_data_vec[i++],&first);
		cp_ethernet_address(_data_vec[i++],&last);
		addChild(&first,&last);
		addParent(&last,&first);
	}
  } else {
	//Hier noch einfügen: Kreise zu Overlay (einfach jedes Paar einfügen, den Rest übernimmt overlay_structure)
	/*Dateiformat:
	* Jeder Kreis eine Zeile
	* Zeilen terminiert durch -1
	* Bsp:
	* 1 2 3 -1
	* 2 4 5 6 7 -1
	*/

	while (i < _data_vec.size()) {
		cp_ethernet_address(_data_vec[i++],&act);
		if (last.is_broadcast()) {
			first=act;
			last=act;
		} else {
			if (act.is_broadcast()) {
				addChild(&last,&first);
				addParent(&first,&last);
			} else {
				addChild(&last,&act);
				addParent(&act,&last);
			}
			last=act;
		}
	}
  }
}

void OverlayStructure::reset() {
	parents.clear();
	children.clear();
	n_parents.clear();
	n_children.clear();
}	

void OverlayStructure::addOwnParent (EtherAddress* add) {
	for (Vector<EtherAddress>::iterator i=parents.begin();i!=parents.end();++i) {
		if ((*i)==(*add)) return;
	}
	parents.push_back(*add);
}

void OverlayStructure::addOwnChild (EtherAddress* add) {
	for (Vector<EtherAddress>::iterator i=children.begin();i!=children.end();++i) {
		if ((*i)==(*add)) return;
	}
	children.push_back(*add);
}

void OverlayStructure::addParent (EtherAddress* node, EtherAddress* add) {
	if (_me->isIdentical(node)) {
		addOwnParent(add);
		return;
	}
	Vector<EtherAddress>* v=n_parents.findp(*node);
	if (v==0) {
		Vector<EtherAddress> newadd;
		newadd.push_back(*add);
		n_parents.insert(*node,newadd);
	} else {
		for (Vector<EtherAddress>::iterator i=v->begin();i!=v->end();++i) {
			if ((*i)==(*add)) return;
		}
		v->push_back(*add);
	}
}

void OverlayStructure::addChild (EtherAddress* node, EtherAddress* add) {
	//BRN_DEBUG("Added Child");
	if (_me->isIdentical(node)) {
		addOwnChild(add);
		return;
	}
	Vector<EtherAddress>* v=n_children.findp(*node);
	if (v==0) {
		Vector<EtherAddress> newadd;
		newadd.push_back(*add);
		n_children.insert(*node,newadd);
	} else {
		for (Vector<EtherAddress>::iterator i=v->begin();i!=v->end();++i) {
			if ((*i)==(*add)) return;
		}
		v->push_back(*add);
	}
}

void OverlayStructure::removeOwnParent (EtherAddress* add) {
	for (Vector<EtherAddress>::iterator i=parents.begin();i!=parents.end();++i) {
		if ((*i)==(*add)) {
			parents.erase(i);
			return;
		}
	}
}

void OverlayStructure::removeOwnChild (EtherAddress* add) {
	for (Vector<EtherAddress>::iterator i=children.begin();i!=children.end();++i) {
		if ((*i)==(*add)) {
			children.erase(i);
			return;
		}
	}
}

void OverlayStructure::removeParent (EtherAddress* node, EtherAddress* add) {
	if (_me->isIdentical(node)) {
		removeOwnParent(add);
		return;
	}
	Vector<EtherAddress>* v=n_parents.findp(*node);
	if (v==0) {
		return;
	} else {
		for (Vector<EtherAddress>::iterator i=v->begin();i!=v->end();++i) {
			if ((*i)==(*add)) {
				v->erase(i);
				return;
			}
		}
	}
}

void OverlayStructure::removeChild (EtherAddress* node, EtherAddress* add) {
	if (_me->isIdentical(node)) {
		removeOwnChild(add);
		return;
	}
	Vector<EtherAddress>* v=n_children.findp(*node);
	if (v==0) {
		return;
	} else {
		for (Vector<EtherAddress>::iterator i=v->begin();i!=v->end();++i) {
			if ((*i)==(*add)) {
				v->erase(i);
				return;
			}
		}
	}
}

Vector <EtherAddress>* OverlayStructure::getOwnParents() {
	return &parents;
}

Vector <EtherAddress>* OverlayStructure::getOwnChildren() {
	return &children;
}

Vector <EtherAddress>* OverlayStructure::getParents(EtherAddress* node) {
	if (_me->isIdentical(node)) {
		return getOwnParents();
	}
	return n_parents.findp(*node);
}

Vector <EtherAddress>* OverlayStructure::getChildren(EtherAddress* node) {
	if (_me->isIdentical(node)) {
		return getOwnChildren();
	}
	return n_children.findp(*node);
}

String OverlayStructure::stats() {
    StringAccum sa;
    sa << "<overlaystructure node=\"" << BRN_NODE_NAME << "\">\n";

    sa << "\t<parents count=\"" << parents.size() << "\" >\n";
    for (Vector<EtherAddress>::iterator i=parents.begin();i!=parents.end();++i) {
        sa << "\t\t<parent name=\"" << i->unparse() << "\"/>\n";
    }
    sa << "\t</parents>\n";

    sa << "\t<childrens count=\"" << children.size() << "\" >\n";
    for (Vector<EtherAddress>::iterator i=children.begin();i!=children.end();++i) {
        sa << "\t\t<children name=\"" << i->unparse() << "\"/>\n";
    }
    sa << "\t</childrens>\n";

    sa << "</overlaystructure>\n";

    return sa.take_string();
}


String OverlayStructure::printOwnParents() {
	StringAccum sa;
	sa << "<parents node=\"" << BRN_NODE_NAME << "\">\n";
	for (Vector<EtherAddress>::iterator i=parents.begin();i!=parents.end();++i) {
		sa << "\t<parent name=\"" << i->unparse() << "\"/>\n";
	}
	sa << "</parents>\n";
	return sa.take_string();
}

String OverlayStructure::printOwnChildren() {
	StringAccum sa;
	sa << "<children node=\"" << BRN_NODE_NAME << "\">\n";
	for (Vector<EtherAddress>::iterator i=children.begin();i!=children.end();++i) {
		sa << "\t<child name=\"" << i->unparse() << "\"/>\n";
	}
	sa << "</children>\n";
	return sa.take_string();
}


//Does not print own parents
String OverlayStructure::printParents(const EtherAddress* add,const Vector<EtherAddress> *par) {
	StringAccum sa;
	sa << "<parents node=\"" << add->unparse() << "\">\n";
	for (Vector<EtherAddress>::const_iterator i=par->begin();i!=par->end();++i) {
		sa << "\t<parent name=\"" << i->unparse() << "\"/>\n";
	}
	sa << "</parents>\n";
	return sa.take_string();
}

//Does not print own children
String OverlayStructure::printChildren(const EtherAddress* add,const Vector<EtherAddress> *par) {
	StringAccum sa;
	sa << "<children node=\"" << add->unparse() << "\">\n";
	for (Vector<EtherAddress>::const_iterator i=par->begin();i!=par->end();++i) {
		sa << "\t<child name=\"" << i->unparse() << "\"/>\n";
	}
	sa << "</children>\n";
	return sa.take_string();
}

String OverlayStructure::printAllParents() {
	StringAccum sa;
	sa << "<overlay_knowledge node=\"" << BRN_NODE_NAME << "\">\n";
		for (HashMap <EtherAddress, Vector <EtherAddress> >::const_iterator i=n_parents.begin();i!=n_parents.end();++i) {
			sa << printParents(&(i.key()),&(i.value()));
		}
	sa << "</overlay_knowledge>\n";
	return sa.take_string();
}

String OverlayStructure::printAllChildren() {
	StringAccum sa;
	sa << "<overlay_knowledge node=\"" << BRN_NODE_NAME << "\">\n";
		for (HashMap <EtherAddress, Vector <EtherAddress> >::const_iterator i=n_children.begin();i!=n_children.end();++i) {
			sa << printChildren(&(i.key()),&(i.value()));
		}
	sa << "</overlay_knowledge>\n";
	return sa.take_string();
}

String OverlayStructure::printPre() {
	StringAccum sa;
	sa << "<pre node=\"" << BRN_NODE_NAME << "\" value=\""<<_pre<<"\"></pre>\n";
	return sa.take_string();
}

/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/

enum {
  H_INFO,
  H_DEBUG,
  H_STATS
};

static String read_stats(Element *e, void */*thunk*/)
{
  OverlayStructure *ovl = (OverlayStructure *)e;
  return ovl->stats();
}

static int add_own_parent (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress address;

  if ( cp_ethernet_address(args[0], &address) ) {
    ovl->addOwnParent(&address);
  }

  return 0;
}

static int add_own_child (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);
  //click_chatter("Adding Child with data: %s",s.c_str());

  EtherAddress address;

  if ( cp_ethernet_address(args[0], &address) ) {
    //click_chatter("Adding Child... %s",address.unparse().c_str());
    ovl->addOwnChild(&address);
  }

  return 0;
}

static int add_parent (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  //Notice that this only makes the node believe, that some node has a new parent, but it's not added to its parent list
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress node, address;

  if ( cp_ethernet_address(args[0], &node) && cp_ethernet_address(args[1], &address)) {
    ovl->addParent(&node,&address);
  }

  return 0;
}

static int add_child (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  //Notice that this only makes the node believe, that some node has a new child, but it's not added to its child list
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress node, address;

  if ( cp_ethernet_address(args[0], &node) && cp_ethernet_address(args[1], &address)) {
    ovl->addChild(&node,&address);
  }

  return 0;
}

static int remove_own_parent (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress address;

  if ( cp_ethernet_address(args[0], &address) ) {
    ovl->removeOwnParent(&address);
  }

  return 0;
}

static int remove_own_child (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress address;

  if ( cp_ethernet_address(args[0], &address) ) {
    ovl->removeOwnChild(&address);
  }

  return 0;
}

static int remove_parent (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  //Notice that this only makes the node believe, that some node has a new parent, but it's not added to its parent list
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress node, address;

  if ( cp_ethernet_address(args[0], &node) && cp_ethernet_address(args[1], &address)) {
    ovl->removeParent(&node,&address);
  }

  return 0;
}

static int remove_child (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/)
{
  //Notice that this only makes the node believe, that some node has a new child, but it's not added to its child list
  OverlayStructure *ovl = (OverlayStructure *)element;

  String s = cp_uncomment(in_s);
  Vector<String> args;
  cp_spacevec(s, args);

  EtherAddress node, address;

  if ( cp_ethernet_address(args[0], &node) && cp_ethernet_address(args[1], &address)) {
    ovl->removeChild(&node,&address);
  }

  return 0;
}

static int set_pre (const String &in_s, Element *element, void */*thunk*/, ErrorHandler */*errh*/) {
	//click_chatter("set_pre0");
	OverlayStructure *ovl = (OverlayStructure *)element;
	
    String s = cp_uncomment(in_s);
    //click_chatter("set_pre1: %s",s.c_str());
    Vector<String> args;
    cp_spacevec(s, args);

    bool new_pre=false; 
	//click_chatter("set_pre2: %s",s.c_str());
	
	if (cp_bool(args[0],&new_pre))
		ovl->_pre=new_pre;
	
	return 0;
}

static int reset_all (const String &/*in_s*/, Element *element, void */*thunk*/, ErrorHandler */*errh*/) {
	OverlayStructure *ovl = (OverlayStructure *)element;
	
    ovl->reset();
	
	return 0;
}

static String
read_own_parents(Element *e, void */*thunk*/)
{
  OverlayStructure *ovl = (OverlayStructure *)e;
  return ovl->printOwnParents();
;
}

static String
read_own_children(Element *e, void */*thunk*/)
{
  OverlayStructure *ovl = (OverlayStructure *)e;
  return ovl->printOwnChildren();
}

static String
read_all_parents(Element *e, void */*thunk*/)
{
  OverlayStructure *ovl = (OverlayStructure *)e;
  return ovl->printAllParents();
}

static String
read_all_children(Element *e, void */*thunk*/)
{
  OverlayStructure *ovl = (OverlayStructure *)e;
  return ovl->printAllChildren();
}

static String
read_pre(Element *e, void */*thunk*/)
{
	OverlayStructure *ovl = (OverlayStructure *)e;
	return ovl->printPre();
}

void OverlayStructure::add_handlers()
{
  add_write_handler("add_own_parent", add_own_parent , (void *)0);
  add_write_handler("add_own_child", add_own_child , (void *)0);
  add_write_handler("add_parent", add_parent , (void *)0);
  add_write_handler("add_child", add_child , (void *)0);
  add_write_handler("remove_own_parent", remove_own_parent , (void *)0);
  add_write_handler("remove_own_child", remove_own_child , (void *)0);
  add_write_handler("remove_parent", remove_parent , (void *)0);
  add_write_handler("remove_child", remove_child , (void *)0);
  add_write_handler("set_pre", set_pre , (void *)0);
  add_write_handler("reset", reset_all , (void *)0);
  add_read_handler("read_own_parents", read_own_parents , (void *)0);
  add_read_handler("read_own_children", read_own_children , (void *)0);
  add_read_handler("read_all_parents", read_all_parents , (void *)0);
  add_read_handler("read_all_children", read_all_children , (void *)0);
  add_read_handler("read_pre",read_pre, (void *)H_INFO);

  add_read_handler("stats", read_stats, (void *)H_STATS);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(OverlayStructure)
