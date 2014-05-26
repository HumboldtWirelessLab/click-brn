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
 * gabriel_graph.{cc,hh} -- computes gabriel graph on the network
 * 
 * WARNING: complexity quadratic in terms of number of neighbours
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
#include "gabriel_graph.hh"

CLICK_DECLS

GabrielGraph::GabrielGraph() 
{
  BRNElement::init();
}

GabrielGraph::~GabrielGraph()
{
}

int
GabrielGraph::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "OVERLAY_STRUCTURE", cpkP+cpkM, cpElement, &_ovl,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
GabrielGraph::initialize(ErrorHandler *)
{
  
  return 0;
}


/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/


void GabrielGraph::add_handlers()
{

}

CLICK_ENDDECLS
EXPORT_ELEMENT(GabrielGraph)
