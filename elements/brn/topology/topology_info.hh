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
#ifndef TOPOLOGY_INFO_HH
#define TOPOLOGY_INFO_HH

#include <click/element.hh>
#include <click/vector.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class TopologyInfo : public BRNElement
{
 public:
    class Bridge {
     public:
      EtherAddress node_a;
      EtherAddress node_b;
    };

    class ArticulationPoint {
     public:
      EtherAddress node;
    };

 public:
  TopologyInfo();
  ~TopologyInfo();

  const char *class_name() const  { return "TopologyInfo"; }

  const char *processing() const  { return AGNOSTIC; }

  const char *port_count() const  { return "0/0"; }

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void add_handlers();

 private:
  Vector<Bridge*> _bridges;
  Vector<ArticulationPoint*> _artpoints;

 public:

  String topology_info(void);

};

CLICK_ENDDECLS
#endif
