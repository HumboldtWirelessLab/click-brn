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
#include "packetarithmetics.hh"
#include <elements/brn/brn.h>
#include "ncsourcebatch.hh"

CLICK_DECLS

int NCSourceBatch::overflowQueueLength = 2 * MAX_FRAGMENTS_IN_BATCH;

NCSourceBatch::NCSourceBatch(uint32_t myId, const click_brn_dsr & route,
		unsigned in_maxSize) :
	NCBatch(myId), NCEncodingBatch(route), sendHeader(myId), latestRoute(route),
			maxSize(in_maxSize) {
	numFragments = 0;
}

NCSourceBatch::NCSourceBatch(NCSourceBatch * previous) :
	NCBatch(previous->nextId()), NCEncodingBatch(previous),
	overflow(previous->overflow), sendHeader(id),
	latestRoute(previous->latestRoute), maxSize(previous->maxSize) 
{
	outgoingRoute = latestRoute;
	previous->overflow.clear();

	for (unsigned i = 0; i < min((unsigned)overflow.size(), maxSize); ++i) {
		if (EXTRA_PACKETS_ANNO(overflow[i]))
			sendHeader.setLastFragment(i);
	}
	unsigned newSize = overflow.size();

	if (newSize > maxSize) {
		for (newSize = maxSize; !(sendHeader.isLastFragment(newSize - 1)); newSize--)
			;
	}

	numFragments = newSize;
	for (unsigned i = 0; i < newSize; ++i) {
		data.push_back(overflow.front());
		overflow.pop_front();
	}
}

NCSourceBatch::~NCSourceBatch() {
	for (int i = 0; i < overflow.size(); ++i) {
		overflow[i]->kill();
	}
}

//add a plain packet
//if it's not a last fragment, put it into overflow
//if it's a last fragment and there is enough space in data and SOFT_TIMEOUT hasn't hit
//append the overflow to data and recalculate the number of packets
//if this is a last fragment and we're out of space or time, clear the overflow

void NCSourceBatch::putPlain(click_brn_dsr & dsrHeader, Packet * packet) {
	latestRoute = dsrHeader;
	if (deliveredEncoded == 0)
		outgoingRoute = dsrHeader;
	putPlain(packet);
}

void NCSourceBatch::putPlain(Packet * packet) {
	overflow.push_back(packet);
	if (EXTRA_PACKETS_ANNO(packet)) {
		// we can readjust the packet number for small batches with large packets here
		// as we haven't sent anything yet
		unsigned newNumFragments = overflow.size() + numFragments;
		if (deliveredEncoded == 0 && newNumFragments <= maxSize) {
			numFragments = newNumFragments;
			sendHeader.setLastFragment(numFragments - 1);
			while (overflow.size()) {
				data.push_back(overflow.front());
				overflow.pop_front();
			}
		} else {
			if (overflow.size() > overflowQueueLength) {
				do {
					overflow.back()->kill();
					overflow.pop_back();
				} while (!EXTRA_PACKETS_ANNO(overflow.back()));
			}
		}
	}
}

//return a random combination of all available packets
Packet * NCSourceBatch::getEncoded() {

	if (numFragments == 0)
		return NULL;

	Packet * sendPacket = NULL;

	if (numFragments == 1) {
		sendHeader.multipliers[0] = 1;
		sendPacket = data[0]->clone()->uniqueify();
	} else {
		sendPacket = Packet::make( 0);
		for (unsigned i = 0; i < numFragments; ++i) {
			int mult = 0;
			int multiplierMask = GaloisConfiguration::instance()->getMultiplierMask();
			while (mult == 0)
				mult = random() & multiplierMask;
			sendHeader.multipliers[i] = mult;
		}
		Packet * tempPacket = Packet::make( 0);
		for (unsigned i = 0; i < numFragments; ++i) {
			memset( ( int8_t * ) tempPacket->data(), 0, tempPacket->length() );
			tempPacket = destructiveAdd(tempPacket, data[i]); //TODO: slow
			tempPacket = destructiveMultiply(sendHeader.multipliers[i], tempPacket);
			sendPacket = destructiveAdd(sendPacket, tempPacket);
		}
		tempPacket->kill();
	}

	sendHeader.fragments_in_batch = numFragments;
	assert(numFragments != 0);
	deliveredEncoded++;
	return addDsrHeader(outgoingRoute, addNcHeader(sendHeader, sendPacket) );
}

#include <click/dequeue.cc>
template class DEQueue<bool>;

CLICK_ENDDECLS

ELEMENT_PROVIDES ( NCSourceBatch )
