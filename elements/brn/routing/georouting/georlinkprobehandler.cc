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
 * GeorLinkProbeHandler.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/etheraddress.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "geortable.hh"
#include "georlinkprobehandler.hh"


CLICK_DECLS

GeorLinkProbeHandler::GeorLinkProbeHandler()
{
  BRNElement::init();
}

GeorLinkProbeHandler::~GeorLinkProbeHandler()
{
}

int
GeorLinkProbeHandler::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LINKSTAT", cpkP+cpkM, cpElement, &_linkstat,
      "GEORTABLE", cpkP+cpkM, cpElement, &_rt,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
tx_handler(void *element, const EtherAddress *ea, char *buffer, int size)
{
  GeorLinkProbeHandler *georh = (GeorLinkProbeHandler*)element;
  return georh->lpSendHandler(buffer, size, ea);
}

static int
rx_handler(void *element, EtherAddress *ea, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  GeorLinkProbeHandler *georh = (GeorLinkProbeHandler*)element;
  return georh->lpReceiveHandler(buffer, size, ea);
}

int
GeorLinkProbeHandler::initialize(ErrorHandler *)
{
  _linkstat->registerHandler(this,BRN2_LINKSTAT_MINOR_TYPE_GEOR,&tx_handler,&rx_handler);
  return 0;
}

int
GeorLinkProbeHandler::lpSendHandler(char *buffer, int size, const EtherAddress */*ea*/)
{
  struct gps_position *pos = (struct gps_position *)buffer;

  if ( size < (int)sizeof(struct gps_position) ) {
    BRN_WARN("No Space for GPSPosition in Linkprobe");
    return 0;
  }

  _rt->getLocalPosition()->getPosition(pos);

  return sizeof(struct gps_position);
}

int
GeorLinkProbeHandler::lpReceiveHandler(char *buffer, int size, EtherAddress *ea)
{
  struct gps_position *pos = (struct gps_position *)buffer;
  _rt->updateEntry(ea, pos);

  return size;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  GeorLinkProbeHandler *fl = (GeorLinkProbeHandler *)e;
  return String(fl->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *, ErrorHandler *errh)
{
  GeorLinkProbeHandler *fl = (GeorLinkProbeHandler *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  fl->_debug = debug;
  return 0;
}

void
GeorLinkProbeHandler::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GeorLinkProbeHandler)
