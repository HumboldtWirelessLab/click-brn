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
#include "ncdestbatch.hh"

CLICK_DECLS


NCDestBatch::~NCDestBatch() {}

Packet * NCDestBatch::getPlain() {

	if (deliverable) {
		Packet * ret = data[columnPositions[deliveredPlain]]->clone()->uniqueify();
		ret = addNcHeader(headers[deliveredPlain], ret);
		if (isBitSet(lastFragments, deliveredPlain)) {
			int deliveredLast = deliveredPlain - 1;
			while (deliveredLast >= 0 && !(isBitSet(lastFragments, deliveredLast)))
				deliveredLast--;
			SET_EXTRA_PACKETS_ANNO(ret, deliveredPlain - deliveredLast);
		} else
			SET_EXTRA_PACKETS_ANNO(ret, 0);
		ret = addDsrHeader(incomingRoute, ret);
		deliveredPlain++;
		deliverable--;
		return ret;
	} else
		return NULL;
}




void NCDestBatch::putEncoded(click_brn_dsr & dsrHeader, click_brn_netcoding & ncHeader, Packet * packet) {

	NCDecodingBatch::putEncoded(dsrHeader, ncHeader, packet);
	if (finished()) {
		deliverable = numFragments - deliveredPlain;
	} else {
		unsigned nextEndFragment = deliveredPlain + deliverable;
		while (nextEndFragment < processedRows) {
			while (!(ncHeader.isLastFragment(nextEndFragment))) {
				++nextEndFragment;
				if (nextEndFragment >= processedRows)
					return;
			}

			for (unsigned pos = deliveredPlain; pos <= nextEndFragment; ++pos) {
				// check if we have a single packet: 0 0 0 ... 0 1 0 ... 0
				unsigned position = columnPositions[pos];
				click_brn_netcoding & header = headers[position];
				for (unsigned i = 0; (i < numFragments); ++i) {
					if (i == position) {
						if (header[position] != 1)
							return;
					} else {
						if (header[i] != 0)
							return;
					}
				}
			}
			deliverable = (++nextEndFragment) - deliveredPlain;
		}
	}
}





void NCDestBatch::swapRows(unsigned row1, unsigned row2) {
	swapHeaders(row1, row2);
	swapPackets(row1, row2);
}

void NCDestBatch::divideRow(unsigned row, unsigned multiplier) {
	assert(multiplier);
	headers[row] /= multiplier;
	data[row] = destructiveDivide(data[row], multiplier);
}


void NCDestBatch::subtractMultipleRow(unsigned base, unsigned subtracted, 
		unsigned multiplier) {
	headers[base] -= (headers[subtracted] * multiplier);
	// TODO: another packet is allocated here; how does the effort for that compare to the
	// gain of using region multiply instead of table multiply?
	Packet * subtract = nondestructiveMultiply(data[subtracted], multiplier);
	data[base] = destructiveSubtract(data[base], subtract);
	subtract->kill();
}

CLICK_ENDDECLS

ELEMENT_PROVIDES ( NCDestBatch )
