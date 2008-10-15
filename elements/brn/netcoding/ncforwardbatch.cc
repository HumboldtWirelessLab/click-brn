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
#include <click/packet_anno.hh>
#include "galois.hh"
#include "ncforwardbatch.hh"
#include "packetarithmetics.hh"
#include <elements/brn/brn.h>

CLICK_DECLS

NCForwardBatch::NCForwardBatch(NCForwardBatch * oldBatch, uint32_t myId) :
	NCBatch(myId),
	NCDecodingBatch(oldBatch),
	NCEncodingBatch(oldBatch)
{
}



NCForwardBatch::NCForwardBatch (uint32_t myId) :
	NCBatch(myId)
{
}



/**
 * add a row to the batch
 */
void NCForwardBatch::addRow ( click_brn_netcoding & header, Packet * p )
{
	
	//TODO: if memory runs low and CPU cycles are available
	// 	we might want to do an independence check here
	//	and discard the packet if it's not independent
	originalHeaders.push_back ( header );
	NCDecodingBatch::addRow(header, p);
}


void NCForwardBatch::swapRows(unsigned n, unsigned k) {
	
	swapHeaders(n,k);
	swapHeaders(originalHeaders, n, k);
	swapPackets(n,k);
	
}




void NCForwardBatch::divideRow(unsigned k, unsigned pivot) {
	assert(pivot);
	headers[k] /= pivot;
}


void NCForwardBatch::subtractMultipleRow(unsigned i, unsigned k, 
		unsigned pivot) {
	headers[i]-=(pivot * headers[k]);
}


void NCForwardBatch::putPlain(click_brn_dsr & dsrHeader, Packet * p) {
	outgoingRoute = dsrHeader;
	putPlain(p);
}

void NCForwardBatch::putPlain( Packet * p) {
	char routePosition = PAINT_ANNO(p); 
	if (routePosition == NODE_OUTSIDE_ROUTE)
		// this node dropped out of the route; stop sending
		incomingRoute.dsr_segsleft = 0;
    
	//packet can be ignored, it shouldn't have changed since getPlain
	p->kill();
}

bool NCForwardBatch::isZero(const click_brn_netcoding & header) {
	for (unsigned i = 0; i < MAX_FRAGMENTS_IN_BATCH; ++i) 
		if (header[i] != 0) return false;
	return true;
}

Packet * NCForwardBatch::getDeepEncoded(click_brn_netcoding & sendHeader) {
	Packet * sendPacket = Packet::make ( 0 );
  Packet * tempPacket = Packet::make ( 0 );
  for ( unsigned i = 0; i < processedRows; ++i )
  {
  	unsigned multiplierMask = GaloisConfiguration::instance()->getMultiplierMask();
   	unsigned mult = 0;
    while ( mult == 0 ) mult = random() & multiplierMask;
    memset ( ( int8_t * ) tempPacket->data(), 0, tempPacket->length() );
    sendHeader += originalHeaders[i] * mult;
    tempPacket = destructiveAdd(tempPacket, data[i]);
    tempPacket = destructiveMultiply(mult, tempPacket);
    sendPacket = destructiveAdd(sendPacket, tempPacket);
  }
  tempPacket->kill();
	return sendPacket;    
}

Packet * NCForwardBatch::getShallowEncoded(click_brn_netcoding & sendHeader) {
	Packet * ret = NULL;
	while (!ret || isZero(sendHeader)) // if none of the received packets was included try again
  {	
  	if (ret != NULL) ret->kill();
   	ret = Packet::make(0);
     	uint32_t r = random();
     	for (unsigned i = 0; i < processedRows; ++i)
     	{
       	if (r & (1 << i))
       	{
       		click_brn_netcoding newHeader = sendHeader + originalHeaders[i];
       		if (!isZero(newHeader)) {
            	sendHeader = newHeader;
             	ret = destructiveAdd(ret, data[i]);
   	    	}
       	}
     	}
    }
  return ret;
}

// choose coefficients so that the rank of the resulting galois polynomial
// doesn't exceed BITS_PER_MULTIPLIER
// Problem with shallow encode: Some multiplier in the header will always have the maximum rank
// so we can only multiply by rank 0 - which means 0 or 1. The best we can do is
// add up a random selection of packets.
// Solution: provide enough bits in header, then deep encoding will be used. 
Packet * NCForwardBatch::getEncoded()
{
	
    if (!stopSent)
        return NULL;
    Packet * ret = NULL;
    click_brn_netcoding sendHeader(id);
    sendHeader.fragments_in_batch = numFragments;
    memcpy(sendHeader.last_fragments, lastFragments, LAST_FRAGMENTS_LENGTH);
    /*if (!multiplierMask & 0xff) {
    	ret = getShallowEncoded(sendHeader);
    } 
    else {*/
    	ret = getDeepEncoded(sendHeader);
    //}
    ret = addNcHeader(sendHeader, ret);
    ret = addDsrHeader(outgoingRoute, ret);
    deliveredEncoded++;
    return ret;
}


#include <click/hashmap.cc>
template class HashMap<Packet *, int>;

CLICK_ENDDECLS

ELEMENT_PROVIDES ( NCForwardBatch )
