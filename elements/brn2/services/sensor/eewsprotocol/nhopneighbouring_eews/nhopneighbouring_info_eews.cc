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
 * NHopNeighbouringInfoEews.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "nhopneighbouring_info_eews.hh"

CLICK_DECLS

NHopNeighbouringInfoEews::NHopNeighbouringInfoEews():
  _hop_limit(DEFAULT_HOP_LIMIT)
{
  BRNElement::init();
}

NHopNeighbouringInfoEews::~NHopNeighbouringInfoEews()
{
}

int
NHopNeighbouringInfoEews::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
	      "EEWSSTATE", cpkP+cpkM , cpElement, &_as,
	      "HOPLIMIT", cpkP, cpInteger, &_hop_limit,
	      "DEBUG", cpkP , cpInteger, &_debug,
	      cpEnd) < 0)
       return -1;

  return 0;
}

int
NHopNeighbouringInfoEews::initialize(ErrorHandler *)
{
  return 0;
}

uint32_t
NHopNeighbouringInfoEews::count_neighbours()
{
  return _ntable.size();
}

//TODO: consider hop_limit and age
uint32_t
NHopNeighbouringInfoEews::count_neighbours(uint32_t hops)
{
  uint32_t n = 0;
  for (NeighbourTableIter iter = _ntable.begin(); iter.live(); iter++)
    if ( iter.value()._hops <= hops ) n++;

  return n;
}

//TODO: consider hop_limit and age
bool
NHopNeighbouringInfoEews::is_neighbour(EtherAddress *ea)
{
  return (_ntable.findp(*ea) != NULL);
}

void
NHopNeighbouringInfoEews::add_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours, GPSPosition *gpspos, uint8_t state)
{
  NeighbourInfoEews *info = _ntable.findp(*ea);
  if ( info == NULL ) {
    _ntable.insert(*ea, NeighbourInfoEews(ea, hops, gpspos, state));
  }
}

void
NHopNeighbouringInfoEews::update_neighbour(EtherAddress *ea, uint8_t hops, uint8_t hop_limit, uint32_t no_neighbours, GPSPosition *gpspos, uint8_t state)
{
  // update_neighbor_table
  NeighbourInfoEews *info = _ntable.findp(*ea);
  if ( info == NULL ) {
    add_neighbour(ea,hops,hop_limit, no_neighbours, gpspos, state);
  } else {
// FIXME: why only for less hops
  if ( info->_hops > hops ) {
      info->update(hops, Timestamp::now(), gpspos, state);
  }

  //update_trigger_table
  // TODO

  // 2 hop fallback
  _as->check_neighbor_alarm(state, ea);


  }
}



String
NHopNeighbouringInfoEews::print_stats()
{
  StringAccum sa;

  sa << "<nhopneighbour_info node=\"" << BRN_NODE_NAME << "\" hop_limit=\"" << (uint32_t)_hop_limit << "\" >\n";
  sa << "\t<neighbours count=\"" << _ntable.size() << "\" >\n";
  for (NeighbourTableIter iter = _ntable.begin(); iter.live(); iter++) {
    NeighbourInfoEews info = iter.value();
    sa << "\t\t<neighbour addr=\"" << info._ea.unparse() << "\" distance=\"" << (uint32_t)info._hops << "\"";
    sa << " last_seen=\"" << info._last_seen.unparse() << "\" />\n";
  }

  sa << "\t</neighbours>\n</nhopneighbour_info>\n";

  return sa.take_string();
}


/*****************************************************************/
/*********************** H A N D L E R ***************************/
/*****************************************************************/
static String
read_state_param(Element *e, void *)
{
  return ((NHopNeighbouringInfoEews *)e)->print_stats();
}

void
NHopNeighbouringInfoEews::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("state", read_state_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NHopNeighbouringInfoEews)