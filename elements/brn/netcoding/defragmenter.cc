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

#include <click/packet_anno.hh>
#include "defragmenter.hh"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

CLICK_DECLS

/**
 * reassembles fragments of plain packets into the original plain packets
 * input: click_brn + click_brn_dsr + click_brn_netcoding + data/n
 * output: click_brn + click_brn_dsr + data
 */
void Defragmenter::push(int, Packet * packet) {
  receivedPackets.push_back(packet);
  if (EXTRA_PACKETS_ANNO(packet)) {
    WritablePacket * result = Packet::make(sizeof(click_brn) + sizeof(click_brn_dsr));
    BRNPacketAnno::set_udevice_anno(result,(BRNPacketAnno::udevice_anno(packet)).c_str());
    unsigned pos = result->length();
    memcpy(result->data(), packet->data(), sizeof(click_brn) + sizeof(click_brn_dsr));
    while (receivedPackets.size()) {
      Packet * fragment = receivedPackets.front();
      receivedPackets.pop_front();
      fragment->pull(sizeof(click_brn) + sizeof(click_brn_dsr) + sizeof(click_brn_netcoding));
      unsigned length = fragment->length();
      result = result->put(length);
      memcpy(result->data() + pos, fragment->data(), length);
      pos += length;
      fragment->kill();
    }
    // this assertion detects invalid ethernet headers if all ethernet headers start with 00
    //assert(*(result->data() + sizeof(click_brn) + sizeof(click_brn_dsr)) == 0);
    
    // We only handle ethernet packets, so we anticipate the ethernet header behind BRN and DSR
    // This makes Align() choke as result->ether_header() > result->data(), so don't get any Align
    // between this and the rest of DSR. 
    result->set_ether_header((click_ether *)(result->data() + sizeof(click_brn) + sizeof(click_brn_dsr)));
    output(0).push(result);
  }
}

CLICK_ENDDECLS

EXPORT_ELEMENT(Defragmenter)
