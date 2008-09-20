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

#ifndef FRAGMENTRECEIVER_HH
#define FRAGMENTRECEIVER_HH

#include <click/config.h>
#include <click/element.hh>
#include <elements/brn/nodeidentity.hh>
#include <elements/brn/common.hh>
#include "netcoding.h"
#include "nccache.hh"
#include "tracecollector.hh"
#include "headerpacker.hh"

CLICK_DECLS

#define FRAGMENT_RECEIVER_INPUT_DATA 0
#define FRAGMENT_RECEIVER_INPUT_STOP 1

#define FRAGMENT_RECEIVER_OUTPUT_DATA 0
#define FRAGMENT_RECEIVER_OUTPUT_ROUTE 1
#define FRAGMENT_RECEIVER_OUTPUT_STOP 2


/**
 * splits encoded packets into fragments and adds click_brn_netcoding headers
 * The input packets should come with click_brn_netcoding_packet at the 
 * beginning of the packet (after click_brn and click_brn_dsr) and
 * click_brn_netcoding_fragment in front of for each fragment.
 */
class FragmentReceiver : public Element {
	public:
		FragmentReceiver() : _debug(BrnLogger::DEBUG), me(NULL), stopSeen(false) {}
		~FragmentReceiver() {}
		int configure(Vector<String> &conf, ErrorHandler *errh);
		const char* class_name() const { return "FragmentReceiver"; }
		const char *port_count() const  { return "2/3"; }
		const char *processing() const { return "PUSH"; }
		void push(int, Packet *);

	private:

		int _debug;
		unsigned fragmentDataLength;
		HeaderPacker * packer;

		// returns:
		// OUTSIDE ROUTE if this node isn't in the route at all
		// <0 if this node is further downstream than intended receiver
		// 0 if this node is the intended receiver
		// >0 if this node is upstream from intended receiver
		char nodeOnRoute(click_brn_dsr * dsrHeader);
		Packet * traceDuplicates(Packet * p, click_brn_dsr * dsrHeader,
		                     char routePosition, click_brn_netcoding & ncHeader);
		NodeIdentity * me;
		NetcodingCache * cache;
		bool stopSeen;
		TraceCollector * tracer;
		bool opportunistic;
};

CLICK_ENDDECLS

#endif

