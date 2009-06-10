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
 * BatmanOriginatorSource.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn2/brnprotocol/brnprotocol.hh"
#include "elements/brn2/brnprotocol/brnpacketanno.hh"
#include "elements/brn2/routing/batman/batmanprotocol.hh"
#include "elements/brn2/brn2.h"

#include "batmanoriginatorsource.hh"

#include "elements/brn/common.hh"


CLICK_DECLS

BatmanOriginatorSource::BatmanOriginatorSource()
  :  _send_timer(static_send_timer_hook,this),
     _debug(BrnLogger::DEFAULT)
{
}

BatmanOriginatorSource::~BatmanOriginatorSource()
{
}

int
BatmanOriginatorSource::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM , cpEtherAddress, &_ether_addr,  //TODO: replace by nodeid and send paket to each (wireless) device
      "INTERVAL", cpkP+cpkM , cpInteger, &_interval,
      "LINKSTAT", cpkP, cpElement, &_linkstat,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
BatmanOriginatorSource::initialize(ErrorHandler *)
{
  _id = 0;
  _send_timer.initialize(this);
  _send_timer.schedule_after_msec(click_random() % (_interval ) );

  return 0;
}

void
BatmanOriginatorSource::set_send_timer()
{
  _send_timer.schedule_after_msec( _interval );
}

void
BatmanOriginatorSource::static_send_timer_hook(Timer *t, void *bos)
{
  if ( t == NULL ) click_chatter("Time is NULL");
  ((BatmanOriginatorSource*)bos)->sendOriginator();
  ((BatmanOriginatorSource*)bos)->set_send_timer();
}

void
BatmanOriginatorSource::sendOriginator()
{
  uint8_t broadcast[] = { 255,255,255,255,255,255 };

  _id++;

  WritablePacket *p = BatmanProtocol::new_batman_originator(_id, 0, &_ether_addr,0);
  WritablePacket *p_brn = BRNProtocol::add_brn_header(p, BRN_PORT_BATMAN, BRN_PORT_BATMAN, 10, DEFAULT_TOS);

  BRNPacketAnno::set_ether_anno(p_brn, _ether_addr, EtherAddress(broadcast), 0x8680);

  output(0).push(p_brn);
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BatmanOriginatorSource *fl = (BatmanOriginatorSource *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  BatmanOriginatorSource *fl = (BatmanOriginatorSource *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
BatmanOriginatorSource::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BatmanOriginatorSource)
