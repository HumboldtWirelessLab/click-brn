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
#include <click/crc32.h>
#include "fragmentreceiver.hh"
#include "elements/brn/standard/brnpacketanno.hh"

CLICK_DECLS

int FragmentReceiver::configure(Vector<String> &conf, ErrorHandler *errh) {
	tracer = NULL;
	fragmentDataLength = 0;
	opportunistic = true;
	if (cp_va_parse(conf, this, errh, cpKeywords, 
			"NODE_IDENTITY", cpElement, "node identity", &me, 
			"NC_CACHE", cpElement, "packet cache", &cache,
			"TRACE_COLLECTOR", cpElement, "trace collector", &tracer,
			"FRAGMENT_LENGTH", cpInteger, "fragment length", &fragmentDataLength,
			"PACKER", cpElement, "header packer",	&packer, 
			"OPPORTUNISTIC", cpBool, "use opportunistic reception", &opportunistic,
			cpEnd) < 0)
		return -1;
	if (!me || !me->cast("NodeIdentity"))
		return -1;
	if (fragmentDataLength == 0)
			return -1;
	if (!cache || !cache->cast("NetcodingCache"))
		return -1;
	if (!packer || !packer->cast("HeaderPacker"))
		return -1;
	return 0;
}
/**
 * returns position of the current node in the route wrt (hop_count - segsleft)
 * >0 => upstream
 * =0 => next hop
 * <0 => downstream
 * to find the previous hop, do i = (hop_count - segsleft + pos - 1)
 * if that's -1, the previous hop is dsr_src, else it's addr[i]
 */
char FragmentReceiver::nodeOnRoute(click_brn_dsr * dsrHeader) {
	int pos = dsrHeader->dsr_hop_count - dsrHeader->dsr_segsleft;
	if (dsrHeader->dsr_src == *(me->getMyWirelessAddress())) {
		return pos + 1;
	}

	for (int i = 0; i < pos; ++i) {
		if (dsrHeader->addr[i].hw == *(me->getMyWirelessAddress())) {
			return pos - i;
		}
	}
	for (int i = pos; i < dsrHeader->dsr_hop_count; ++i) {
		if (dsrHeader->addr[i].hw == *(me->getMyWirelessAddress())) {
			dsrHeader->dsr_segsleft = dsrHeader->dsr_hop_count - i;
			return pos - i;
		} else {
			dsrHeader->addr[i].metric = htons(BRN_DSR_INVALID_ROUTE_METRIC);
		}
	}
	// this node isn't in the route anymore (or it's the destination); 
	// a forwardbatch will recognize this and not generate any credit
	dsrHeader->dsr_segsleft = 0;
	if (dsrHeader->dsr_dst == *(me->getMyWirelessAddress())) {
		return pos - dsrHeader->dsr_hop_count;
	}

	return NODE_OUTSIDE_ROUTE;
}

// receive a packet, unpack the netcoding headers, send fragments
void FragmentReceiver::push(int port, Packet * packet) {
	if (port == FRAGMENT_RECEIVER_INPUT_STOP) {
		stopSeen = true;
		click_ether * ether = (click_ether *)packet->mac_header();
		for (int i = 0; i < 6; ++i)
			ether->ether_shost[i] = me->getMyWirelessAddress()->data()[i];
		output(FRAGMENT_RECEIVER_OUTPUT_STOP).push(packet);
	} else {
		stopSeen = false;
		if (packet->length() < sizeof(click_brn) + sizeof(click_brn_dsr)
				+ sizeof(click_brn_netcoding_packet)) {
			// packet is extremely truncated - nothing to do here
			packet->kill();
			return;
		}
		click_brn brnHeader = *(click_brn *)(packet->data());
		packet->pull(sizeof(click_brn));
		click_brn_dsr dsrHeader = *(click_brn_dsr *)(packet->data());

		click_brn_netcoding_packet ncPacketHeader =
				*(click_brn_netcoding_packet *)(packet->data() + sizeof(click_brn_dsr));
		if ((uint32_t)(update_crc(0xffffffff, (char *) packet->data(),
				sizeof(click_brn_dsr) + sizeof(click_brn_netcoding_packet) - 4))
				!= ncPacketHeader.crc) {
			BRN_DEBUG("killing packet with wrong header checksum");
			packet->kill();
			return;
		}

		char routePosition = nodeOnRoute(&dsrHeader);
		if (routePosition == NODE_OUTSIDE_ROUTE) {
			if (!cache->batchExists(ncPacketHeader.batch_id)) {
				packet->kill();
				return;
			} else {
				BRN_DEBUG("route change occured. This node is no longer used.");
			}
		} else if (routePosition > 0) {
			// we're upstream from sender
			packet->kill();
			return;
		} else if (routePosition != 0 && !opportunistic) {
			// opportunistic reception switched off and we're downstream from direct 
			// successor
			packet->kill();
			return;
		}

		packet->pull(sizeof(click_brn_dsr) + sizeof(click_brn_netcoding_packet));
		click_brn_netcoding ncHeader;
		ncHeader.batch_id = ncPacketHeader.batch_id;
		//don't care about endianness here, as long as batch_id is unique
		memcpy(&(ncHeader.last_fragments), &(ncPacketHeader.last_fragments), LAST_FRAGMENTS_LENGTH);
		//endianness matters here
		ncHeader.fragments_in_batch = ncPacketHeader.fragments_in_batch;
    String udevice = BRNPacketAnno::udevice_anno(packet);
    
		unsigned numFragments = 0;
		unsigned packetLength = packet->length();
		unsigned fragmentRawLength = fragmentDataLength + 4 + packer->getHeaderLength();
		
		while (packetLength > 0 && !stopSeen && routePosition != NODE_OUTSIDE_ROUTE) {
			unsigned fragmentLength = fragmentRawLength;
			if (packetLength <= fragmentRawLength) {
				fragmentLength = packetLength;
			}
			if (fragmentLength < fragmentRawLength) {
				// fragment is truncated; don't try to decode
				break;
			}
			WritablePacket * out = Packet::make(fragmentDataLength
					+ sizeof(click_brn_netcoding) + sizeof(click_brn_dsr)
					+ sizeof(click_brn) + 4);
			// 4 because of CRC
			memcpy(out->data(), &brnHeader, sizeof(click_brn));
			memcpy(out->data() + sizeof(click_brn), &dsrHeader, sizeof(click_brn_dsr));

			packer->unpack(packet, ncHeader);
			memcpy(out->data() + sizeof(click_brn) + sizeof(click_brn_dsr),
					&ncHeader, sizeof(click_brn_netcoding));

			memcpy(out->data() + sizeof(click_brn) + sizeof(click_brn_dsr)
					+ sizeof(click_brn_netcoding), packet->data(), fragmentDataLength
					+ 4);
      BRNPacketAnno::set_udevice_anno(out,udevice.c_str());
			out->set_timestamp_anno(Timestamp::now());
			SET_PAINT_ANNO(out, routePosition);
			output(FRAGMENT_RECEIVER_OUTPUT_DATA).push(out);
			numFragments++;
			packet->pull(fragmentDataLength + 4);
			packetLength = packet->length();
		}

		if (stopSeen && tracer) {
			packet = traceDuplicates(packet, &dsrHeader, routePosition, ncHeader);
		}

		if (numFragments > 0 || routePosition == NODE_OUTSIDE_ROUTE) {
			// if all the fragments are broken we don't send a route update
			// if the node isn't on the route, we do in order to mark it as non-encoding
			WritablePacket * route = packet->push(sizeof(click_brn)
					+ sizeof(click_brn_dsr) + sizeof(click_brn_netcoding));
			memcpy(route->data(), &brnHeader, sizeof(click_brn));
			memcpy(route->data() + sizeof(click_brn), &dsrHeader,
					sizeof(click_brn_dsr));
			memcpy(route->data() + sizeof(click_brn) + sizeof(click_brn_dsr),
					&ncHeader, sizeof(click_brn_netcoding));
			SET_PAINT_ANNO(route, routePosition);
			SET_EXTRA_PACKETS_ANNO(route, numFragments);
			route->set_timestamp_anno(Timestamp::now());
			output(FRAGMENT_RECEIVER_OUTPUT_ROUTE).push(route);
		}
		stopSeen = false;
	}
}

Packet * FragmentReceiver::traceDuplicates(Packet * packet,
		click_brn_dsr * dsrHeader, char routePosition,
		click_brn_netcoding & ncHeader) {
	unsigned packetLength = packet->length();
	unsigned fragmentRawLength = fragmentDataLength + 4 + packer->getHeaderLength();
	while (packetLength > 0) {
		unsigned fragmentLength = fragmentRawLength;
		if (packetLength <= fragmentRawLength) {
			fragmentLength = packetLength;
		}
		if (fragmentLength < fragmentRawLength) {
			// fragment is truncated; don't try to decode
			break;
		}

		packer->unpack(packet, ncHeader);
		packet = packet->push(sizeof(click_brn_netcoding));

		memcpy((char *)packet->data(), &ncHeader, sizeof(click_brn_netcoding));

		unsigned length = fragmentDataLength + sizeof(click_brn_netcoding);

		unsigned pcrc;
		memcpy(&pcrc, packet->data() + length, 4);
		// batch id, fragments in batch, last fragments are not checked.
		unsigned crc = update_crc(0xffffffff, (char *) packet->data(), length);
		if (pcrc == crc) {
			tracer->duplicate(dsrHeader, routePosition);
		} else {
			tracer->corrupt(dsrHeader, routePosition);
		}
		packet->pull(length + 4);
		packetLength = packet->length();
	}
	return packet;
}

CLICK_ENDDECLS

EXPORT_ELEMENT(FragmentReceiver)
