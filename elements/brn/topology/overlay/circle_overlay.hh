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

#ifndef CIRCLE_OVERLAY_HH
#define CIRCLE_OVERLAY_HH
#include <click/element.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <clicknet/ether.h>

#include "elements/brn/brnelement.hh"
#include <elements/brn/routing/identity/brn2_nodeidentity.hh>
#include "overlay_structure.hh"

CLICK_DECLS

class CircleOverlay : public BRNElement {

 public:
  CircleOverlay();
  ~CircleOverlay();

  const char *class_name() const  { return "CircleOverlay"; }
  const char *port_count() const  { return "0/0"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return false; }
  int initialize(ErrorHandler *);

  void add_handlers();

  EtherAddress ID_to_MAC (int id);
  int MAC_to_ID(EtherAddress *add);

 private:

  BRN2NodeIdentity *_me;
  OverlayStructure *_ovl;
  String _circle_path;
  void get_neighbours(String path);

};

CLICK_ENDDECLS
#endif
