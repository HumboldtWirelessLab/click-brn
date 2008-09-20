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

#ifndef PACKETARITHMETICS_HH
#define PACKETARITHMETICS_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/timer.hh>
#include "netcoding.h"
#include "headerpacker.hh"

CLICK_DECLS

// returns the next long word aligned address after a
uint8_t * upperBound(uint8_t * a);


class GaloisConfiguration : public BitPacker {
	public:
		static GaloisConfiguration * instance();
		
		// configures the galois library for fields of bits length
		void configure(unsigned bits);
		
		uint32_t getMultiplierMask() {return multiplierMask;}
		
		// multiply region with mult with best method possible; intended for NC headers
		// if length % sizeof(int) != 0 behavior is undefined
		void multiply(uint8_t * region, unsigned mult, unsigned length) {
			multiply(region, mult, length, sizeof(unsigned) * 8);
		}
		
		// divide region by div with best method possible; intended for NC headers
		// if length % sizeof(int) != 0 behavior is undefined 
		void divide(uint8_t * region, unsigned div, unsigned length) {
			divide(region, div, length, sizeof(unsigned) * 8);
		}
		
		// multiply or divide region, but assume it's packed. Intended for data
		// if bitLength % 8 != 0 then the fields are assumed to be stuffed across 
		// byte borders. This is going to be ungodly slow.
		// also, if using odd bit lengths and if the param length * 8 is no clean 
		// multiple of that, undefined behavior ensues.
		// if bitsInMultiplier is 8, 16 or 32 regular divide or multiply is called
		void packedMultiply(uint8_t * region, unsigned mult, unsigned length);
		void packedDivide(uint8_t * region, unsigned div, unsigned length);
		
		
	private:
		GaloisConfiguration() {};
		uint32_t multiplierMask;
		void multiply(uint8_t * start, unsigned mult, unsigned length, 
				unsigned multiplierLength);
		void divide(uint8_t * region, unsigned div, unsigned length, 
				unsigned multiplierLength);
		void singleMultiply(uint8_t * front, unsigned multby, unsigned length, 
				unsigned multiplierLength);
		void (*regionMultiply)(char *region, int multby, int nbytes, char *r2, int add);
};

//adds packets and overwrites a. Like operator+=, but a may be killed and recreated
Packet * destructiveAdd(Packet * a, Packet * b);
//adds packets and creates a new one. Like operator+
Packet * nondestructiveAdd(Packet * a, Packet * b);
// like operator*
Packet * nondestructiveMultiply(unsigned mult, Packet * a);
// like operator*=
Packet * destructiveMultiply(unsigned mult, Packet * a);

inline Packet * nondestructiveMultiply(Packet * a, unsigned mult)
	{return nondestructiveMultiply(mult, a);}

inline Packet * destructiveMultiply(Packet * a, unsigned mult)
	{return destructiveMultiply(mult, a);}

Packet * nondestructiveDivide(Packet * a, unsigned div);

Packet * destructiveDivide(Packet * a, unsigned div);

//syntactic sugar. Add is the same as Subtract in galois fields
inline Packet * destructiveSubtract(Packet * a, Packet * b)
	{return destructiveAdd(a,b);}

inline Packet * nondestructiveSubtract(Packet * a, Packet * b)
	{return nondestructiveAdd(a,b);}

bool operator==(const click_brn_netcoding & a, const click_brn_netcoding & b);

bool operator!=(const click_brn_netcoding & a, const click_brn_netcoding & b);

click_brn_netcoding operator+(const click_brn_netcoding & a, const click_brn_netcoding & b);

click_brn_netcoding & operator+=(click_brn_netcoding & a, const click_brn_netcoding & b);

click_brn_netcoding operator-(const click_brn_netcoding & a, const click_brn_netcoding & b);

inline click_brn_netcoding & operator-=(click_brn_netcoding & a, const click_brn_netcoding & b)
	{return a+=b;}

click_brn_netcoding & operator*=(click_brn_netcoding & a, unsigned b);

click_brn_netcoding operator*(const click_brn_netcoding & a, unsigned b);

inline click_brn_netcoding operator*(unsigned b, const click_brn_netcoding & a)
	{return a*b;}

click_brn_netcoding & operator/=(click_brn_netcoding & a, unsigned b);

click_brn_netcoding operator/(const click_brn_netcoding & a, unsigned b);

void adjustStateFields(click_brn_netcoding & target, const click_brn_netcoding & a, const click_brn_netcoding & b);

CLICK_ENDDECLS

#endif
