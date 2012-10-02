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
 * mobility.{cc,hh}
 */

#include <click/config.h>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#if CLICK_NS
#include <click/router.hh>
#endif

#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "mobility.hh"

CLICK_DECLS

Mobility::Mobility()
{
  BRNElement::init();
}

Mobility::~Mobility()
{
}

int
Mobility::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
       return -1;

  return 0;
}

int
Mobility::initialize(ErrorHandler *)
{
  return 0;
}

#if CLICK_NS
void
Mobility::move(int x, int y, int z, int speed, int move_type)
{
  int pos[4];

  switch ( move_type ) {
    case MOVE_TYPE_ABSOLUTE: {
      pos[0] = x;
      pos[1] = y;
      pos[2] = z;
      pos[3] = speed;
      break;
    }
    case MOVE_TYPE_RELATIVE: {
      simclick_sim_command(router()->simnode(), SIMCLICK_GET_NODE_POSITION, &pos);
      pos[0] += x;
      pos[1] += y;
      pos[2] += z;
      pos[3] = speed;
      break;
    }
    default: return;
  }

  simclick_sim_command(router()->simnode(), SIMCLICK_SET_NODE_POSITION, &pos);
}
#else
void
Mobility::move(int, int, int, int)
{
}
#endif


//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {
  H_MOVE
};

static int
write_position_param(const String &in_s, Element *e, void *thunk, ErrorHandler */*errh*/)
{
  int x,y,z,speed;
  Mobility *mob = (Mobility *)e;
  switch ((uintptr_t) thunk)
  {
    case H_MOVE: {
      String s = cp_uncomment(in_s);
      Vector<String> args;
      cp_spacevec(s, args);

      cp_integer(args[0], &x);
      cp_integer(args[1], &y);
      cp_integer(args[2], &z);
      cp_integer(args[3], &speed);

      mob->move(x,y,z,speed);
    }

  }

  return 0;
}

void
Mobility::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("move", write_position_param, H_MOVE);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Mobility)
