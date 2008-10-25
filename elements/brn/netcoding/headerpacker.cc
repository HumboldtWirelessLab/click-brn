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
#include "headerpacker.hh"

CLICK_DECLS

int HeaderPacker::configure(Vector<String> &conf, ErrorHandler *errh) {
	fragmentsInBatch = 0;
	bitsInMultiplier = 0;
	if (cp_va_kparse(conf, this, errh,
      "FRAGMENTS_IN_BATCH", cpkP+cpkM, cpInteger, /*"fragments in batch",*/ &fragmentsInBatch,
			"BITS_IN_MULTIPLIER", cpkP+cpkM, cpInteger, /*"bits in multiplier",*/ &bitsInMultiplier,
			cpEnd) < 0)
		return -1;
	if (fragmentsInBatch == 0)
		return -1;
	if (bitsInMultiplier == 0)
		return -1;
	unsigned headerBitLength = bitsInMultiplier * fragmentsInBatch;
	headerLength = headerBitLength / 8 + (headerBitLength % 8 ? 1 : 0);
	return 0;
}

WritablePacket * HeaderPacker::pack(WritablePacket * dest, const click_brn_netcoding & source) {
	unsigned pos = dest->length();
	dest = dest->put(headerLength);
	uint8_t * multipliers = (uint8_t *)(dest->data() + pos);
	for (unsigned i = 0; i < fragmentsInBatch; ++i) {
		put(i, source[i], multipliers);
	}
	return dest;
}

void HeaderPacker::unpack(Packet * source, click_brn_netcoding & dest) {
	uint8_t * multipliers = (uint8_t *)(source->data());
	for (unsigned i = 0; i < fragmentsInBatch; ++i) {
		dest[i] = get(i, multipliers);
	}
	source->pull(headerLength);
}

unsigned BitPacker::get(unsigned i, const uint8_t * multipliers) {
	unsigned ret = 0;
	unsigned offset0 = i * bitsInMultiplier;
	for (unsigned offset = offset0; offset < offset0 + bitsInMultiplier; ++offset) {
		unsigned offset_in_source_byte = offset % 8;
		unsigned offset_in_target_byte = offset - offset0;
		unsigned source_byte = offset / 8;
		int shift_val = offset_in_source_byte - offset_in_target_byte;
		if (shift_val >= 0)
			ret |= ( (multipliers[source_byte] & ( 1 << offset_in_source_byte ) )
					>> shift_val);
		else
			ret |= ( (multipliers[source_byte] & ( 1 << offset_in_source_byte ) )
					<< -shift_val);
	}
	return ret;
}

void BitPacker::put(unsigned i, unsigned val, uint8_t * multipliers) {
	unsigned offset0 = i * bitsInMultiplier;
	for (unsigned offset = offset0; offset < offset0 + bitsInMultiplier; ++offset) {
		unsigned offset_in_target_byte = offset % 8;
		unsigned offset_in_source_byte = offset - offset0;
		unsigned target_byte = offset / 8;
		int shift_val = offset_in_target_byte - offset_in_source_byte;
		multipliers[target_byte] &= ~ ( 1 << offset_in_target_byte );
		if (shift_val >= 0)
			multipliers[target_byte] |= ((val & ( 1 << offset_in_source_byte ))
					<< shift_val);
		else
			multipliers[target_byte] |= ((val & ( 1 << offset_in_source_byte ))
					>> -shift_val);
	}
}

CLICK_ENDDECLS

EXPORT_ELEMENT(HeaderPacker)
