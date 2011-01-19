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

#include "nhopneighbouring_protocol.hh"
#include "nhopneighbouring_ping.hh"

CLICK_DECLS

NHopNeighbouringPing::NHopNeighbouringPing()
  :_timer(this),
   _interval(DEFAULT_NHOPNEIGHBOURING_PING_INTERVAL),
   _active(true)
{
  BRNElement::init();
}

NHopNeighbouringPing::~NHopNeighbouringPing()
{
}

int
NHopNeighbouringPing::configure(Vector<String> &conf, ErrorHandler* errh)
{

  if (cp_va_kparse(conf, this, errh,
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
NHopNeighbouringPing::initialize(ErrorHandler *)
{
  _timer.initialize(this);

  click_srandom(_node_identity->getMasterAddress()->hashcode());

  _timer.schedule_after_msec( _interval + ( click_random() % _interval ));

  return 0;
}

void
NHopNeighbouringPing::run_timer(Timer */*t*/)
{
  send_ping();
  _timer.schedule_after_msec(_interval);
}

void
NHopNeighbouringPing::send_ping()
{
  WritablePacket *p = NHopNeighbouringProtocol::new_ping(_node_identity->getMasterAddress(), nhop_info->count_neighbours(),
                                                         nhop_info->get_hop_limit());
  if (p) {
    output(0).push(p);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
NHopNeighbouringPing::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(NHopNeighbouringPing)
