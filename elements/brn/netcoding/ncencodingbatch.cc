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
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "ncencodingbatch.hh"
#include "ncencoder.hh"
#include "packetarithmetics.hh"

CLICK_DECLS



NCEncodingBatch::~NCEncodingBatch() {}



NCEncodingBatch::NCEncodingBatch(NCEncodingBatch * other) :
	deliveredEncoded ( 0 ),
	outgoingRoute(other->outgoingRoute),
	stopReceived(false),
	startReceived(false),
	scheduled(false)
{
	if (outgoingRoute.dsr_segsleft == 0) 
		startReceived = true;	
}

NCEncodingBatch::NCEncodingBatch(const click_brn_dsr & route) :
	deliveredEncoded(0),
	outgoingRoute(route),
	stopReceived(false),
	startReceived(true),
	scheduled(false)
{
}

NCEncodingBatch::NCEncodingBatch() :
	deliveredEncoded(0),
	stopReceived(false),
	startReceived(true),
	scheduled(false)
{
	outgoingRoute.dsr_type = BRN_DSR_RERR;
}
	


CLICK_ENDDECLS
ELEMENT_PROVIDES(NCEncodingBatch)
