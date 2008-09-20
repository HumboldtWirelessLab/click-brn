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

#ifndef HEADERPACKER_HH
#define HEADERPACKER_HH

#include <click/config.h>
#include <click/element.hh>
#include <elements/brn/nodeidentity.hh>
#include <elements/brn/common.hh>
#include "netcoding.h"
#include "nccache.hh"
#include "tracecollector.hh"

CLICK_DECLS

class BitPacker {
	protected:
		unsigned get(unsigned i, const uint8_t * multipliers);
		void put(unsigned i, unsigned val, uint8_t * multipliers);
		unsigned bitsInMultiplier;
};

class HeaderPacker : public BitPacker, public Element {
	public:
		int configure(Vector<String> &conf, ErrorHandler *errh);
		const char *class_name() const {return "HeaderPacker";}
		
		/**
		 * packs the source header into an array of multipliers which is appended to
		 * the given packet. Returns the new packet.
		 */
		WritablePacket * pack(WritablePacket * dest, const click_brn_netcoding & source);
		
		/**
		 * unpacks an array of multipliers expected at the head of the given packet 
		 * into the given netcoding header. Pulls the multipliers afterwards.
		 */
		void unpack(Packet * source, click_brn_netcoding & dest);
		
		unsigned getHeaderLength() {return headerLength;}
		
	private:
		
		unsigned fragmentsInBatch;
		unsigned headerLength;

};

CLICK_ENDDECLS
#endif
