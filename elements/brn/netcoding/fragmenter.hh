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

#ifndef FRAGMENTER_HH
#define FRAGMENTER_HH

#include <click/config.h>
#include <click/element.hh>
#include "netcoding.h"

CLICK_DECLS
/*
 * Fragmenting:
 * - packets between fragmenting and coding always include click_brn, 
 *	click_brn_dsr and click_brn_netcoding in that order, for the last fragment 
 * 	in a packet LAST_FRAGMENT_IN_PACKET is set.
 * - after stripping click_brn_dsr the cache peeks into click_brn_netcoding to 
 * 	see if this is a forwarding (not NETCODING_INVALID) batch
 * - for each received packet, the FragmentReceiver sends out a route update
 * - a route update consists of click_brn, click_brn_dsr and 
 * 	click_brn_netcoding, in that order, without any data following it
 * - dsr learns routes from the route update and updates the metrics in 
 * 	click_brn_dsr
 * - route updates bypass the decoder, the defragmenter and the fragmenter
 * 	and are directly fed back into the respective batch via Encoder/cache.
 * - encoder and destinationBatch "burst", always draw or provide enough packets 
 * 	for the fragmentsender or defragmenter to assemble a complete packet
 * 
 * possible optimizations: 
 * - include click_brn and click_brn_dsr only in last fragments, have the cache 
 * 	wait for last fragment before assigning a batch
 * - split up click_brn_netcoding, so that only the necessary bits are actually 
 * 	allocated
 */


/**
 * splits plain packets into fragments for encoder
 */
class Fragmenter : public Element {
	public:
		Fragmenter() {}
		~Fragmenter() {}
		int configure(Vector<String> &conf, ErrorHandler *errh);
		const char* class_name() const { return "Fragmenter"; }
		const char *port_count() const  { return "1/1"; }
		const char *processing() const { return "PUSH"; }
		void push(int, Packet *);

	private:
		unsigned fragmentDataLength;
};

CLICK_ENDDECLS

#endif
