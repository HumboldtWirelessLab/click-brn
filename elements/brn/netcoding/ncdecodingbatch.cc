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
#include "galois.hh"
#include "ncdecodingbatch.hh"
#include "packetarithmetics.hh"
#include <elements/brn/brn.h>

CLICK_DECLS

NCDecodingBatch::NCDecodingBatch()  : 
	receivedEncoded(0), deliveredPlain(0), processedRows(0), 
	stopSent(false), startSent(false), ackable(false) 
{
	incomingRoute.dsr_type = BRN_DSR_RERR;
	init();
}

void NCDecodingBatch::init() {
	for ( int i = 0; i < MAX_FRAGMENTS_IN_BATCH; ++i )
	{
		columnPositions[i] = i;
	}
	for (int i = 0; i < LAST_FRAGMENTS_LENGTH; ++i) {
		lastFragments[i] = 0;
	}
}

NCDecodingBatch::NCDecodingBatch(NCDecodingBatch * old) :
	receivedEncoded(0), deliveredPlain(0), processedRows(0),
	stopSent(false), startSent(false), ackable(false), incomingRoute(old->incomingRoute) 
{
	init();
}

NCDecodingBatch::~NCDecodingBatch() {}



void NCDecodingBatch::addRow ( click_brn_netcoding & header, Packet * packet )
{
	
    //TODO: if memory runs low and CPU cycles are available
    // 	we might want to do an independence check here
    //	and discard the packet if it's not independent
    headers.push_back ( header );
    data.push_back ( packet );
    click_brn_netcoding & insertedHeader = headers.back();

    sortColumns(insertedHeader);

    // zero out the already calculated columns
    for ( unsigned row = 0; row < processedRows; ++row )
    {
    	unsigned pivot = insertedHeader[row];
        subtractMultipleRow(receivedEncoded - 1, row, pivot);
        assert(insertedHeader[row] == 0);
    }
}

void NCDecodingBatch::putEncoded(click_brn_dsr & dsrHeader, 
		click_brn_netcoding & ncHeader, Packet * packet) {
	assert(!finished());
	unsigned oldProcessed = processedRows;
	if (receivedEncoded > 0) {
		assert (numFragments == ncHeader.fragments_in_batch);
	} else {
		numFragments = ncHeader.fragments_in_batch;
		memcpy(lastFragments, ncHeader.last_fragments, LAST_FRAGMENTS_LENGTH);
	}
	receivedEncoded++;
	
	incomingRoute = dsrHeader;
	addRow(ncHeader, packet);
	trySolve();
	
	ackable = (oldProcessed == processedRows);
}

void NCDecodingBatch::sortColumns(click_brn_netcoding & header) {
	
	click_brn_netcoding origHeader = header;
	unsigned position = 0;
	for ( unsigned i = 0; i < numFragments; ++i )
	{
		if ( ( position = columnPositions[i] ) != i )
		{
			header[position] = origHeader[i];
		}
	}
}



void NCDecodingBatch::swapColumns(unsigned n, unsigned k) {
	
	//swap in another column
	unsigned pos1 = columnPositions[n];
	unsigned pos2 = columnPositions[k];
	unsigned oldval = columnPositions[pos1];
	columnPositions[pos1] = columnPositions[pos2];
	columnPositions[pos2] = oldval;
	for ( unsigned l = 0; l < receivedEncoded; l++ )
	{
		unsigned tmpd = headers[l][k];
		headers[l][k] = headers[l][n];
		headers[l][n] = tmpd;
	}
}

bool NCDecodingBatch::eliminateZeroPivot ( unsigned k )
{
	
	// first step of the kth iteration
	// we need to ensure that the pivot is not zero
	if ( headers[k][k] != 0 )
		return true;
	else
	{
		for ( unsigned n = k+1 ; n < receivedEncoded ; n++ )
		{
			if ( headers[n][k] != 0 )
			{
				swapRows ( n,k );
				assert(headers[k][k] > 0  && 
						headers[k][k] <= GaloisConfiguration::instance()->getMultiplierMask());
				return true;
			}
		}

		for ( unsigned n = k+1 ; n < numFragments ; n++ )
		{
			if ( headers[k][n] != 0 )
			{
				swapColumns ( n,k );
				assert(headers[k][k] > 0 && 
						headers[k][k] <= GaloisConfiguration::instance()->getMultiplierMask());
				return true;
			}
		}

		return false;
	}
}

bool NCDecodingBatch::trySolve()
{
	
	unsigned k = processedRows, i = 0;
	unsigned pivot = 0;

	while (k < min(numFragments, receivedEncoded))
	{
		if (!eliminateZeroPivot(k)) {
			return false;
		}
		//  - we can check for k useful packets here (not interesting for forwarder)
		//  - we don't have to recalculate them afterwards (so start at processedRows)
		//  - we don't need to search the same packets for non-zero pivots again (ditto)
		//	- mind new packets, they're not zeroed in already calculated columns, yet
		//		(subtract the calculated headers at addRow)
		
		// cycle A
		// divide row k by pivot, make headers[k][k] 1
		// why start at k, not at 0? Because leading non-zeroes have been eliminated by cycle B
		pivot = headers[k][k];
		assert(pivot != 0 && pivot <= GaloisConfiguration::instance()->getMultiplierMask());
		divideRow(k, pivot);
		assert(headers[k][k] == 1);

		// cycle B
		// Make all other rows 0 in column k.
		// There are no  arithmetic operations on columns, only addition and substraction
		// of multiples of rows. That means, we can add more rows later on without
		// modifying them (other than exchanging columns by columnPositions).
		for ( i = 0; i < receivedEncoded ; i++ )
		{
			// loop over all rows
			if ( i != k )
			{
				pivot = headers[i][k];
				subtractMultipleRow(i, k, pivot);
			}
		}
		processedRows = ++k;
	}
	return processedRows == numFragments;
}

bool NCDecodingBatch::isSelfContained() {
	if (finished()) return true;
	for (unsigned row = 0; row < receivedEncoded; ++row) {
		for (unsigned col = processedRows; col < numFragments; ++col)
			if(headers[row][col] != 0)
				return false;
	}
	return true; 
}

void NCDecodingBatch::swapHeaders(unsigned row1, unsigned row2) {
	swapHeaders(headers, row1, row2);
}


void NCDecodingBatch::swapHeaders(Vector<click_brn_netcoding> & h, 
		unsigned row1, unsigned row2) {
	click_brn_netcoding tmp = h[row1];
	h[row1] = h[row2];
	h[row2] = tmp;
}


void NCDecodingBatch::swapPackets(unsigned row1, unsigned row2) {
	Packet * tmpp = data[row1];
	data[row1] = data[row2];
	data[row2] = tmpp;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(NCDecodingBatch)
