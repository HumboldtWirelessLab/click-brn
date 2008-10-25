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
 * pushbrnheader.{cc,hh} -- element pushes (leading) BRN header at offset position 0.
 */

#include <click/config.h>
#include "elements/brn/brn.h"
#include "pushbrnheader.hh"
CLICK_DECLS

PushBRNHeader::PushBRNHeader()
{
}

PushBRNHeader::~PushBRNHeader()
{
}

Packet *
PushBRNHeader::simple_action(Packet *p)
{

  int brn_size = sizeof(click_brn);
  if ( p->push(brn_size) != NULL )
    return p;
  else
  {
    p->kill();
    click_chatter("Error on push in pushbrnheader");
    return NULL;
  }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PushBRNHeader)
ELEMENT_MT_SAFE(PushBRNHeader)
