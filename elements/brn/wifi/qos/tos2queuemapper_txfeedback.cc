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
 * tos2queuemapper_txfeedback.{cc,hh}
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "tos2queuemapper_txfeedback.hh"

CLICK_DECLS

Tos2QueueMapperTXFeedback::Tos2QueueMapperTXFeedback():
    _tos2qm(NULL)
{
  BRNElement::init();
}

Tos2QueueMapperTXFeedback::~Tos2QueueMapperTXFeedback()
{
}

int
Tos2QueueMapperTXFeedback::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "TOS2QM", cpkP+cpkM, cpElement, &_tos2qm,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0) return -1;

  return 0;
}

Packet *
Tos2QueueMapperTXFeedback::simple_action(Packet *p)
{
    _tos2qm->handle_feedback(p);
    return p;
}

void
Tos2QueueMapperTXFeedback::add_handlers()
{
  BRNElement::add_handlers(); //for Debug-Handlers
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Tos2QueueMapperTXFeedback)
ELEMENT_MT_SAFE(Tos2QueueMapperTXFeedback)
