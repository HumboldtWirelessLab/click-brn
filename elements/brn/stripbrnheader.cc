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
 * stripbrnheader.{cc,hh} -- element removes (leading) BRN header at offset position 0.
 */

#include <click/config.h>
#include "brn.h"
#include "stripbrnheader.hh"
CLICK_DECLS

StripBRNHeader::StripBRNHeader()
{
}

StripBRNHeader::~StripBRNHeader()
{
}

Packet *
StripBRNHeader::simple_action(Packet *p)
{

  int brn_size = sizeof(click_brn);
  p->pull(brn_size);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StripBRNHeader)
ELEMENT_MT_SAFE(StripBRNHeader)
