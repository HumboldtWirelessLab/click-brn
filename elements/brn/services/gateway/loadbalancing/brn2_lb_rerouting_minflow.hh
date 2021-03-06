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

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef LB_REROUTING_MINFLOW_HH
#define LB_REROUTING_MINFLOW_HH

#include <click/element.hh>
#include "brn2_lb_rerouting.hh"

CLICK_DECLS

class LoadbalancingReroutingMinFlow : public LoadbalancingRerouting {
  public:
    LoadbalancingReroutingMinFlow();
    ~LoadbalancingReroutingMinFlow();

    const char *class_name() const  { return "LoadbalancingReroutingMinFlow"; }

    void *cast(const char *name);

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    const char *lb_routing_name() const { return "LoadbalancingReroutingMinFlow"; }

    EtherAddress *getBestNodeForFlow();

};

CLICK_ENDDECLS
#endif
