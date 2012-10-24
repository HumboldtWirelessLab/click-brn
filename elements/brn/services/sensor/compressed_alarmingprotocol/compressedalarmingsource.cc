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
 * CompressedAlarmingSource.{cc,hh}
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/brn2.h"

#include "compressedalarmingsource.hh"
#include "compressedalarmingstate.hh"
#include "compressedalarmingprotocol.hh"

CLICK_DECLS

CompressedAlarmingSource::CompressedAlarmingSource()
{
  BRNElement::init();
}

CompressedAlarmingSource::~CompressedAlarmingSource()
{
}

int
CompressedAlarmingSource::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "ALARMINGSTATE", cpkP+cpkM , cpElement, &_as,
      "DEBUG", cpkP , cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
CompressedAlarmingSource::initialize(ErrorHandler *)
{
  return 0;
}

void
CompressedAlarmingSource::send_alarm(uint8_t alarm_type)
{
  WritablePacket *p = CompressedAlarmingProtocol::new_alarming_packet(alarm_type);

  output(0).push(p);
}


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {H_ALARM};

static int
CompressedAlarmingSource_write_param(const String &in_s, Element *e, void *vparam,
                             ErrorHandler *errh)
{
  CompressedAlarmingSource *as = (CompressedAlarmingSource *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_ALARM: {    //debug
      int alarm;
      if (!cp_integer(s, &alarm))
        return errh->error("debug parameter must be integer");
      as->send_alarm(alarm);
      break;
    }
  }

  return 0;
}

void
CompressedAlarmingSource::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("state", CompressedAlarmingSource_write_param,(void *) H_ALARM);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CompressedAlarmingSource)
