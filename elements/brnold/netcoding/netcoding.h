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

#ifndef CLICK_NETCODING_NETCODING_H_
#define CLICK_NETCODING_NETCODING_H_

#include <elements/brn/brn.h>
#include <click/etheraddress.hh>

/**
 * netcoding header:
 * ... | ethernet header | click_brn | click_brn_dsr | click_brn_netcoding | ...
 * This layout is necessary because the netcoding system sits between DSR and BRNDS.
 * BRNDS, however peeks into click_brn_dsr and prepends the ethernet header. So I
 * either need to change BRNDS or I need to keep the layout of ethernet, brn and
 * dsr header as it is. Of course this goes against the concept of layers.
 * click_brn.dst_port is 10 for netcoding data packets, 11 for START and STOP
 */
CLICK_DECLS


#define MAX_QUEUE_LENGTH 8

#define BRN_PORT_NETCODING 10
#define BRN_PORT_NETCODING_STOP 11
#define NETCODING_INVALID 0xffffffff

//TODO: is that predefined somewhere?
#define BYTE_ALIGN 4
#define NODE_OUTSIDE_ROUTE (-BRN_DSR_MAX_HOP_COUNT -2)

// these are maximum values, only used as dimension for the internal headers.
// The dimensions of the galois fields and external headers, as well as how many
// of the provided fields are actually used are configured at run time.
#define MAX_FRAGMENTS_IN_BATCH 256
#define LAST_FRAGMENTS_LENGTH (MAX_FRAGMENTS_IN_BATCH / 8 + (MAX_FRAGMENTS_IN_BATCH % 8 ? 1 : 0))  

#define PADDING_LENGTH(frags) ((BYTE_ALIGN - ((frags * sizeof(int) + 8 + LAST_FRAGMENTS_LENGTH) % BYTE_ALIGN)) % BYTE_ALIGN)

// if EXTRA_PACKETS_ANNO != 0 then the cache interprets this fragment as
// the last in a packet; the fragmenter doesn't know the batch layout so this workaround is necessary
//
// the same mechanism is used to signal the decoder that it should send a route update to the DSR
// the FragmentReceiver splits up a packet and marks the last fragment. The decoder determines
// if the fragment belongs to a forwarding batch and sends a route update accordingly

inline bool isBitSet(const uint8_t * const fragments, unsigned i) {
		const uint8_t & fragByte = fragments[i/8];
		return fragByte & (1 << i % 8);
}

struct click_brn_netcoding
{
	unsigned & operator[] ( unsigned i )
	{
		return multipliers[i];
	}
	const unsigned & operator[](unsigned i) const {
		return multipliers[i];
	}
	click_brn_netcoding()
	{
		memset(this, 0, sizeof(click_brn_netcoding));
		batch_id = NETCODING_INVALID;
		fragments_in_batch = MAX_FRAGMENTS_IN_BATCH;
	}
	click_brn_netcoding(uint32_t batch)
	{
		memset(this, 0, sizeof(click_brn_netcoding));
		batch_id = batch;
		for (unsigned i = 0; i < LAST_FRAGMENTS_LENGTH; ++i)
			last_fragments[i] = 0;
		fragments_in_batch = MAX_FRAGMENTS_IN_BATCH;
	}
	
	bool valid() const
	{
		return batch_id != NETCODING_INVALID;
	}

	bool isLastFragment(unsigned i) const {
		return isBitSet(last_fragments, i);
	}

	void setLastFragment(unsigned i) {
		uint8_t & fragByte = last_fragments[i/8];
		fragByte |= (1 << i % 8);
	}

	bool lastFragmentSeen() const {
		for (unsigned i = 0; i < LAST_FRAGMENTS_LENGTH; ++i) {
			if (last_fragments[i] != 0) return true;
		}
		return false;
	}

	uint32_t batch_id;
	uint32_t fragments_in_batch;
	uint8_t last_fragments[LAST_FRAGMENTS_LENGTH];
	unsigned multipliers[MAX_FRAGMENTS_IN_BATCH];

	// compiler adds a random padding automatically, so we better do that explicitly to avoid
	// failing CRC; originally this should be eliminated with CLICK_CXX_PROTECT, but all the 
	// convenience of get(), put(), contructors, operator[] would be lost ... :(
	uint8_t padding[PADDING_LENGTH(MAX_FRAGMENTS_IN_BATCH)];
};

// the general netcoding header is packed into fragment and packet headers by the FragmentSender
// and unpacked by the FragmentReceiver
//
// Fragment headers are just tightly packet bit salad, so they aren't declared here.
// HeaderPacket knows how to deal with them
typedef struct click_brn_netcoding_packet {
	bool isLastFragment(unsigned i) const {
		return isBitSet(last_fragments, i);
	}

	void setLastFragment(unsigned i) {
		uint8_t & fragByte = last_fragments[i/8];
		fragByte |= (1 << i % 8);
	}

	bool lastFragmentSeen() const {
		for (unsigned i = 0; i < 32; ++i) {
			if (last_fragments[i] != 0)
				return true;
		}
		return false;
	}
	uint32_t batch_id;
	uint32_t fragments_in_batch;
	uint8_t last_fragments[LAST_FRAGMENTS_LENGTH];
	uint8_t padding[BYTE_ALIGN - ((12 + LAST_FRAGMENTS_LENGTH) % BYTE_ALIGN)];
	uint32_t crc; // crc32 over dsr and netcoding_packet header
} click_brn_netcoding_packet;

inline bool operator==(const EtherAddress & a, const hwaddr & b) {
	const unsigned char * adata = a.data();
	for (unsigned i = 0; i < 6; ++i) {
		if (adata[i] != b.data[i])
			return false;
	}
	return true;
}

inline bool operator==(const EtherAddress & a, const uint8_t * b) {
	const unsigned char * adata = a.data();
	for (unsigned i = 0; i < 6; ++i) {
		if (adata[i] != b[i])
			return false;
	}
	return true;
}

inline bool operator==(const hwaddr & a, const hwaddr & b) {
	for (unsigned i = 0; i < 6; ++i) {
		if (a.data[i] != b.data[i])
			return false;
	}
	return true;
}

inline bool operator!=(const hwaddr & a, const hwaddr & b) {
	return ( ! (a == b ) );
}

inline bool operator==(const hwaddr & a, const EtherAddress & b) {
	return (b == a );
}

inline bool operator!=(const hwaddr & a, const EtherAddress & b) {
	return ( ! (b == a ) );
}

inline bool operator!=(const EtherAddress &a, const hwaddr &b) {
	return ( ! (a == b ) );
}

inline int hashcode(const hwaddr& a) {
	// the last four bytes typically show the most variance, I guess
	// because there are no vendor codes
	// TODO: verify, improve
	int32_t ret = * ( reinterpret_cast<const int32_t *> (a.data + 2 ) );
	return ret;
}
CLICK_ENDDECLS

#endif /*CLICK_NETCODING_NETCODING_H_*/
