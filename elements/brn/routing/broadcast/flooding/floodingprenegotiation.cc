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
 * FloodingPrenegotiation.{cc,hh}
 */

#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include "elements/brn/routing/linkstat/brn2_brnlinkstat.hh"
#include "elements/brn/standard/compression/lzw.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "floodingprenegotiation.hh"


CLICK_DECLS

FloodingPrenegotiation::FloodingPrenegotiation()
{
  BRNElement::init();
}

FloodingPrenegotiation::~FloodingPrenegotiation()
{
}

int
FloodingPrenegotiation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
//      "LINKSTAT", cpkP+cpkM , cpElement, &_linkstat,
      cpEnd) < 0)
       return -1;

  return 0;
}

static int
tx_handler(void *element, const EtherAddress */*ea*/, char *buffer, int size)
{
  FloodingPrenegotiation *lph = (FloodingPrenegotiation*)element;

  return lph->lpSendHandler(buffer, size);
}

static int
rx_handler(void *element, EtherAddress */*ea*/, char *buffer, int size, bool /*is_neighbour*/, uint8_t /*fwd_rate*/, uint8_t /*rev_rate*/)
{
  FloodingPrenegotiation *lph = (FloodingPrenegotiation*)element;

  return lph->lpReceiveHandler(buffer, size);
}

int
FloodingPrenegotiation::initialize(ErrorHandler *errh)
{
  return 0;
}

int
FloodingPrenegotiation::lpSendHandler(char *buffer, int size)
{
  return 0;
}

int
FloodingPrenegotiation::lpReceiveHandler(char *buffer, int size)
{
  return 0;
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

void
FloodingPrenegotiation::add_handlers()
{
  BRNElement::add_handlers();  
}

CLICK_ENDDECLS
EXPORT_ELEMENT(FloodingPrenegotiation)
