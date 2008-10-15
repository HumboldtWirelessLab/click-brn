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

#include "packetarithmetics.hh"
#include "galois.hh"
#include <elements/brn/common.hh>

CLICK_DECLS

#define SOL sizeof(long)
#define ULG unsigned long

GaloisConfiguration * GaloisConfiguration::instance() {
	static GaloisConfiguration singleton;
	return & singleton;
}
		

// configures the galois library for fields of bits length 
void GaloisConfiguration::configure(unsigned bits) {
	bitsInMultiplier = bits;
	for (unsigned i = 0; i < bits; ++i)
		multiplierMask |= (1 << i);
	switch(bits) {
		case 8:
			regionMultiply = galois_w08_region_multiply;
			break;
		case 16:
			regionMultiply = galois_w16_region_multiply;
			break;
		case 32:
			regionMultiply = galois_w32_region_multiply;
			break;
		default:
			regionMultiply = 0;
	}
	
}
		

void GaloisConfiguration::singleMultiply(uint8_t * data, unsigned mult, 
		unsigned length, unsigned multiplierLength) {
	switch(multiplierLength) {
		case 8:
			for (uint8_t * base = (uint8_t *)data; (uint8_t *)base < data + length; base++) {
					*base = galois_single_multiply(*base, mult, bitsInMultiplier);
			}
			break;
		case 16:
			for (uint16_t * base = (uint16_t *)data; (uint8_t *)base < data + length; base++) {
					*base = galois_single_multiply(*base, mult, bitsInMultiplier);		
			}
			break;
		case 32:
			for (uint32_t * base = (uint32_t *)data; (uint8_t *)base < data + length; base++) {
					*base = galois_single_multiply(*base, mult, bitsInMultiplier);		
			}
			break;
		default:
			assert(false);
	}
}

void GaloisConfiguration::multiply(uint8_t * start, unsigned mult, 
		unsigned length, unsigned multiplierLength) {
	if (regionMultiply != 0) {
		uint8_t * region = upperBound(start);
		int frontDiff = region - start;
		int regionLength = length - frontDiff;
		regionLength = (regionLength / SOL) * SOL;
		int backDiff = length - regionLength - frontDiff;
		if ((frontDiff * 8) % multiplierLength || regionLength < 0) {
			singleMultiply(start, mult, length, multiplierLength);
		} else {
			regionMultiply((char *)region, mult, regionLength, 0, 0);
			singleMultiply(start, mult, frontDiff, multiplierLength);
			singleMultiply(region + regionLength, mult, backDiff, multiplierLength);
		}	
	} else {
		singleMultiply(start, mult, length, multiplierLength);
	}
}
		
// divide region by div with best method possible
void GaloisConfiguration::divide(uint8_t * region, unsigned div, 
		unsigned length, unsigned multiplierLength) {
	multiply(region, galois_inverse(div, bitsInMultiplier), length, multiplierLength);
}
		
// multiply or divide region, but assume it's packed.
// if bitsInMultiplier % 8 != 0 then the fields are assumed to be stuffed across 
// byte borders. This is going to be ungodly slow.
void GaloisConfiguration::packedMultiply(uint8_t * region, unsigned mult, 
		unsigned length) {
	if (bitsInMultiplier == 8 || bitsInMultiplier == 16 || bitsInMultiplier == 32) {
		//can be done without bit fiddling, as bits fill up a whole field 
		multiply(region, mult, length, bitsInMultiplier);
	} else {
		for (unsigned i = 0; bitsInMultiplier * i < length * 8; ++i) {
			unsigned val = galois_single_multiply(get(i, region), mult, bitsInMultiplier);
			put(i, val, region);
		}
	}
}

void GaloisConfiguration::packedDivide(uint8_t * region, unsigned div, unsigned length) {
	packedMultiply(region, galois_inverse(div, bitsInMultiplier), length);
}
	
uint8_t * upperBound(uint8_t * a) {
	uint8_t * ret = (uint8_t *)((((ptrdiff_t)a)/SOL) * SOL);
	if (ret != a) ret += SOL;
	return ret;
}



Packet * destructiveAdd ( Packet * a, Packet * b )
{
	int lengthA = a->length();
	int lengthB = b->length();
	char * dataA;
	Packet * ret = NULL;
	if ( lengthB > lengthA )
	{
		ret = a->put ( lengthB - lengthA );
		dataA = ( char * ) ret->data();
		memset ( dataA + lengthA, 0, lengthB - lengthA );
	}
	else
	{
		dataA = ( char * ) a->data();
		ret = a;
	}
	char * dataB = ( char * ) b->data(); 

	if (!((ULG)dataA % SOL) && !((ULG)dataB % SOL) && !(lengthB % SOL)) {
		galois_region_xor ( dataA, dataB, dataA, lengthB );
	} else {
		// galois library requires longword aligned data for region operations
		for (int i = 0; i < lengthB; ++i)
			dataA[i] = dataA[i] ^ dataB[i];
	}
	return ret;
}

Packet * nondestructiveAdd ( Packet * a, Packet * b )
{
	Packet * ret =  a->clone()->uniqueify();
	return destructiveAdd ( ret, b );
}

Packet * destructiveMultiply ( unsigned mult, Packet * a )
{
	
	unsigned lengthA = a->length();
	uint8_t * dataA = ( uint8_t * ) a->data();
	
	GaloisConfiguration::instance()->packedMultiply(dataA, mult, lengthA);
	
	return a;
}

Packet * nondestructiveMultiply ( unsigned mult, Packet * a )
{
	Packet * ret =  a->clone()->uniqueify();
	return destructiveMultiply ( mult, ret );
}

void adjustStateFields(click_brn_netcoding & target, const click_brn_netcoding & a, const click_brn_netcoding & b) {
	target.fragments_in_batch = min(a.fragments_in_batch, b.fragments_in_batch);
	if (a.batch_id != b.batch_id) 
		target.batch_id = NETCODING_INVALID;
	else
		target.batch_id = a.batch_id;
	for (unsigned i = 0; i < LAST_FRAGMENTS_LENGTH; ++i)
		target.last_fragments[i] = 
			max(a.last_fragments[i], b.last_fragments[i]);
}

bool operator== ( const click_brn_netcoding & a, const click_brn_netcoding & b )
{
	if ( &a == &b )
		return true;
	if ( a.batch_id != b.batch_id )
		return false;
	for ( unsigned i = 0; i < MAX_FRAGMENTS_IN_BATCH; ++i )
	{
		if ( a.multipliers[i] != b.multipliers[i] )
			return false;
	}
	return true;
}

bool operator!= ( const click_brn_netcoding & a, const click_brn_netcoding & b )
{
	return ! ( a == b );
}

void addHeaders(click_brn_netcoding & ret, const click_brn_netcoding & a, const click_brn_netcoding & b) {
	adjustStateFields(ret, a, b);
	char * am = ( char * ) a.multipliers;
	char * bm = ( char * ) b.multipliers;
	char * rm = ( char * ) ret.multipliers;
	if (!((MAX_FRAGMENTS_IN_BATCH * sizeof(int)) % SOL) && !((ULG)am % SOL) && !((ULG)bm % SOL) && !((ULG)rm % SOL))
		galois_region_xor ( am, bm, rm, MAX_FRAGMENTS_IN_BATCH * sizeof(int) );
	else {
		for (unsigned i = 0; i < MAX_FRAGMENTS_IN_BATCH; ++i)
			ret.multipliers[i] = a.multipliers[i] ^ b.multipliers[i];
	}
}

click_brn_netcoding operator+ ( const click_brn_netcoding & a, const click_brn_netcoding & b )
{
	click_brn_netcoding ret;
	addHeaders(ret, a, b);
	return ret;
}

click_brn_netcoding & operator+= ( click_brn_netcoding & a, const click_brn_netcoding & b )
{
	adjustStateFields(a,a,b);
	addHeaders(a,a,b);
	return a;
}

click_brn_netcoding & operator/= ( click_brn_netcoding & a, unsigned b )
{
	
	uint8_t * am = (uint8_t *) a.multipliers;
	GaloisConfiguration::instance()->divide(am, b, MAX_FRAGMENTS_IN_BATCH * sizeof(int));
	return a;
}

click_brn_netcoding operator* ( const click_brn_netcoding & a, unsigned b )
{
	click_brn_netcoding ret ( a );
	return ( ret *= b );
}

click_brn_netcoding & operator*= ( click_brn_netcoding & a, unsigned b )
{
	uint8_t * am = (uint8_t *) a.multipliers;
	GaloisConfiguration::instance()->multiply(am, b, MAX_FRAGMENTS_IN_BATCH * sizeof(int));
	return a;
}

Packet * nondestructiveDivide ( Packet * a, unsigned div )
{
	Packet * ret =  a->clone()->uniqueify();
	return destructiveDivide ( ret, div );
}

Packet * destructiveDivide ( Packet * a, unsigned div )
{
	unsigned lengthA = a->length();
	uint8_t * dataA = ( uint8_t * ) a->data();
	GaloisConfiguration::instance()->packedDivide(dataA, div, lengthA);
	return a;
}


#undef ULG
#undef SOL

CLICK_ENDDECLS

ELEMENT_PROVIDES ( PacketArithmetics )
