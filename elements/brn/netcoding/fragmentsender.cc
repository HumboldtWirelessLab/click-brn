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
#include <click/crc32.h>
#include <click/packet_anno.hh>
#include "fragmentsender.hh"

CLICK_DECLS

int FragmentSender::configure(Vector<String> &conf, ErrorHandler *errh) {
	if (cp_va_kparse(conf, this, errh, cpKeywords, 
			"PACKER",	cpElement, "header packer", &packer,
			cpEnd) < 0)
		return -1;
	if (!packer || !packer->cast("HeaderPacker"))
		return -1;
	return 0;
}

void FragmentSender::push(int, Packet * packet) {
	receivedPackets.push_back(packet);
	if (EXTRA_PACKETS_ANNO(packet) != 0) {
		click_brn_netcoding * ncHeader = (click_brn_netcoding *)(packet->data()
				+ sizeof(click_brn) + sizeof(click_brn_dsr));
		WritablePacket * result = Packet::make(sizeof(click_brn)
				+ sizeof(click_brn_dsr) + sizeof(click_brn_netcoding_packet));
		unsigned pos = result->length();
		memcpy(result->data(), packet->data(), sizeof(click_brn)
				+ sizeof(click_brn_dsr));
		click_brn_netcoding_packet * resultHeader =
				(click_brn_netcoding_packet *) (result->data() + sizeof(click_brn)
						+ sizeof(click_brn_dsr));
		resultHeader->batch_id = ncHeader->batch_id;
		resultHeader->fragments_in_batch = ncHeader->fragments_in_batch;
		memcpy(resultHeader->last_fragments, ncHeader->last_fragments, LAST_FRAGMENTS_LENGTH);
		resultHeader->crc = update_crc(0xffffffff, (char *) result->data()
				+ sizeof(click_brn), sizeof(click_brn_dsr)
				+ sizeof(click_brn_netcoding_packet) - 4);
		while (receivedPackets.size()) {
			Packet * fragment = receivedPackets.front();
			receivedPackets.pop_front();
			fragment->pull(sizeof(click_brn) + sizeof(click_brn_dsr));
			ncHeader = (click_brn_netcoding *)fragment->data();
			result = packer->pack(result, *ncHeader);
			pos = result->length();
			unsigned length = fragment->length() - sizeof(click_brn_netcoding);
			result = result->put(length);
			
			fragment->pull(sizeof(click_brn_netcoding));
			memcpy(result->data() + pos, fragment->data(), length);
			pos += length;
			fragment->kill();
		}
		output(0).push(result);
	}
}

CLICK_ENDDECLS

EXPORT_ELEMENT(FragmentSender)
