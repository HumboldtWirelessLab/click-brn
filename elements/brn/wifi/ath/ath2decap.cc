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
#include "ath2decap.hh"
#include "ah_desc.h"
CLICK_DECLS

Ath2Decap::Ath2Decap()
{
}

Ath2Decap::~Ath2Decap()
{
}

Packet *
Ath2Decap::simple_action(Packet *p)
{
  int ath2_size = sizeof(struct ath_desc_status);
  p->pull(ath2_size);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Decap)
ELEMENT_MT_SAFE(Ath2Decap)
