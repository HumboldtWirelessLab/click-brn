/* Copyright (C) 2008 Ulf Hermann
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
 */

#include <click/config.h>
#include <click/glue.hh>
#include "ncbatch.hh"

CLICK_DECLS

/**
 * calculate previous id
 */
uint32_t NCBatch::prevId(uint32_t orig) {
	return orig - 1;
}

/**
 * calculate next id
 */
uint32_t NCBatch::nextId() const {
	return id + 1;  
}

Packet * NCBatch::addDsrHeader(const click_brn_dsr & inHeader, Packet * p_in)
{
  WritablePacket * p = p_in->push(sizeof(click_brn_dsr));
  int8_t * data = (int8_t *)p->data();
  click_brn_dsr * dsrHeader = (click_brn_dsr *) (data);
  *dsrHeader = inHeader;
  return p;
}

Packet * NCBatch::addNcHeader(const click_brn_netcoding & inHeader, Packet * p_in)
{
  WritablePacket * p = p_in->push(sizeof(click_brn_netcoding));
  int8_t * data = (int8_t *)p->data();
  click_brn_netcoding * ncHeader = (click_brn_netcoding *) (data);
  *ncHeader = inHeader;
  return p;
}

NCBatch::~NCBatch()
{
  for ( int i = 0 ; i < data.size() ; ++i )
  {
    data[i]->kill();
  }
}

ELEMENT_PROVIDES(NCBatch)

CLICK_ENDDECLS
