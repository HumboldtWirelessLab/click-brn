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
 * rn_graph.{cc,hh} -- computes relative neighbourhood graph on the network
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
#include "rn_graph.hh"

CLICK_DECLS

RNGraph::RNGraph() :
	_timer(static_calc_neighbors,this), _me(NULL),_ovl(NULL),_link_table(NULL),_update_intervall(0),_threshold(0)
{
  BRNElement::init();
}

RNGraph::~RNGraph()
{
	_timer.unschedule();
}

int
RNGraph::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_me,
      "OVERLAY_STRUCTURE", cpkP+cpkM, cpElement, &_ovl,
      "LINKTABLE", cpkP+cpkM, cpElement, &_link_table,
      "UPDATE_INTERVALL", cpkP+cpkM, cpInteger, &_update_intervall,
      "THRESHOLD", cpkP+cpkM, cpInteger, &_threshold,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

uint32_t
RNGraph::metric2dist_sqr(uint32_t metric)
{
  /*assume ETX-Metrik proportional to distance*/
  return metric;
}

void
RNGraph::calc_neighbors() {
	_timer.unschedule();
	_neighbors.clear();
	EtherAddress _my_ether_add=*(_me->getMasterAddress());
	_link_table->get_neighbors(_my_ether_add, _neighbors);
	_ovl->reset();
	for (Vector<EtherAddress>::iterator i=_neighbors.begin();i!=_neighbors.end();++i) {
		bool useable=true;
		for (Vector<EtherAddress>::iterator k=_neighbors.begin();k!=_neighbors.end();++k) {
			if (*i==*k) continue;
			uint32_t d_mei=metric2dist_sqr(_link_table->get_link_metric(_my_ether_add,*i));
			uint32_t d_mek=metric2dist_sqr(_link_table->get_link_metric(_my_ether_add,*k));
			uint32_t d_ki=metric2dist_sqr(_link_table->get_link_metric(*k,*i));
			if (d_mei>d_mek+_threshold&&d_mei>d_ki+_threshold) {
				useable=false;
				break;
			}
		}
		if (useable) {
			//_neighbors.push_back(*i);
			_ovl->addOwnChild(i);
			_ovl->addParent(i,&_my_ether_add);
		}
	}
    Timestamp _next = Timestamp::now() + Timestamp::make_msec(_update_intervall);
    _timer.schedule_at(_next);
}

int
RNGraph::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  Timestamp _next = Timestamp::now() + Timestamp::make_msec(_update_intervall);
  _timer.schedule_at(_next);
  return 0;
}


/*************************************************************************************************/
/***************************************** H A N D L E R *****************************************/
/*************************************************************************************************/


void RNGraph::add_handlers()
{

}

CLICK_ENDDECLS
EXPORT_ELEMENT(RNGraph)
