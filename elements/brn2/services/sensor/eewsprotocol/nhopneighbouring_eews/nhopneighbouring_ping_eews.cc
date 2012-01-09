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
 * eventnotifier.{cc,hh}
 */

#include <click/config.h>

#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "nhopneighbouring_protocol_eews.hh"
#include "nhopneighbouring_ping_eews.hh"

CLICK_DECLS

NHopNeighbouringPingEews::NHopNeighbouringPingEews()
  :_timer(this),
   _interval(DEFAULT_NHOPNEIGHBOURING_PING_EEWS_INTERVAL),
   _active(true)
{
  BRNElement::init();
}

NHopNeighbouringPingEews::~NHopNeighbouringPingEews()
{
}

int
NHopNeighbouringPingEews::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
      "EEWSSTATE", cpkP+cpkM , cpElement, &_as,
      "NODEIDENTITY", cpkP+cpkM, cpElement, &_node_identity,
      "NHOPN_INFO", cpkP+cpkM, cpElement, &nhop_info,
      "INTERVAL", cpkP, cpInteger, &_interval,
      "ACTIVE", cpkP, cpBool, &_active,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
NHopNeighbouringPingEews::initialize(ErrorHandler *)
{
  _timer.initialize(this);

  click_srandom(_node_identity->getMasterAddress()->hashcode());

  _timer.schedule_after_msec( _interval + ( click_random() % _interval ));

  return 0;
}

void
NHopNeighbouringPingEews::run_timer(Timer */*t*/)
{
  send_ping();
  _timer.schedule_after_msec(_interval);
}

void
NHopNeighbouringPingEews::send_ping()
{
	const EtherAddress *src = _node_identity->getMasterAddress();
	GPSPosition *gpspos =  _as->getPosition();
	uint8_t state =  _as->getState();
	uint32_t num_neighbors = nhop_info->count_neighbours();
	uint32_t hop_limit = nhop_info->get_hop_limit();

  if ( gpspos == NULL ) {
    BRN_ERROR("No valid position");
    return;
  }

  WritablePacket *p = NHopNeighbouringProtocolEews::new_ping(src, num_neighbors,
                                                         hop_limit, gpspos, state);
  BRN_DEBUG("Create ping Packet by eth:%s loc:%s state:%d num-neighbors: %d hop-limit: %d", src->unparse().c_str(), gpspos->unparse_coord().c_str(), state, num_neighbors, hop_limit);

  if (p) {
    output(0).push(p);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
NHopNeighbouringPingEews::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NHopNeighbouringPingEews)
