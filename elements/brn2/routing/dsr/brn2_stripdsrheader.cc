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
 * stripdsrheader.{cc,hh} -- element removes DSR header
 */

#include <click/config.h>
#include "brn2_stripdsrheader.hh"
#include "brn2_dsrprotocol.hh"
CLICK_DECLS

BRN2StripDSRHeader::BRN2StripDSRHeader()
{
}

BRN2StripDSRHeader::~BRN2StripDSRHeader()
{
}

Packet *
BRN2StripDSRHeader::simple_action(Packet *p)
{

  int dsr_size = DSRProtocol::header_length(p);
  p->pull(dsr_size);
  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2StripDSRHeader)
ELEMENT_MT_SAFE(BRN2StripDSRHeader)
