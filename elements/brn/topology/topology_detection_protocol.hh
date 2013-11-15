/* Copyright (C) 2005 BerlinRoofNet Lab
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
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/* Sender-/Receivernumbers */
/* field: sender,receiver */
#ifndef TOPOLOGY_DETECTION_PROTOCOL_HH
#define TOPOLOGY_DETECTION_PROTOCOL_HH

#include <click/element.hh>

#include "topology_info.hh"

CLICK_DECLS

struct td_header {
  uint8_t type;
  uint32_t id;
  uint8_t src[6];
  uint8_t ttl;
  uint8_t entries;
};

#define TD_LINK_FLAG_IS_BRIGDE 1

struct td_link {
  uint8_t src[6];
  uint8_t dst[6];
  uint8_t metric;
  uint8_t flags;
};

class TopologyDetectionLink {
  public:
    EtherAddress _a;
    EtherAddress _b;

    bool _is_bridge;

    TopologyDetectionLink() {
      _is_bridge = false;
    }

    TopologyDetectionLink(EtherAddress *a, EtherAddress *b, bool is_bridge) {
      _a = *a;
      _b = *b;
      _is_bridge = is_bridge;
    }

    void serialize(struct td_link *tdl) {
      memcpy(tdl->src,_a.data(),6);
      memcpy(tdl->dst,_b.data(),6);
      if ( _is_bridge ) tdl->flags = tdl->flags | TD_LINK_FLAG_IS_BRIGDE;
      else tdl->flags = tdl->flags & (!TD_LINK_FLAG_IS_BRIGDE);
    }

    void unserialize(struct td_link *tdl) {
      _a = EtherAddress(tdl->src);
      _b = EtherAddress(tdl->dst);
      _is_bridge = (( tdl->flags & TD_LINK_FLAG_IS_BRIGDE ) == TD_LINK_FLAG_IS_BRIGDE);
    }

};

class TopologyDetectionProtocol {

  public:

    static WritablePacket *new_detection_packet(const EtherAddress *src, uint32_t id, uint8_t ttl);
    static WritablePacket *fwd_packet(Packet *p, const EtherAddress *src, EtherAddress *new_node);
    static uint8_t* get_info(Packet *p, EtherAddress *src, uint32_t *id, uint8_t *n_entries, uint8_t *ttl, uint8_t *type);

    static WritablePacket *new_backwd_packet(EtherAddress *td_src, uint32_t td_id, const EtherAddress *src, EtherAddress *dst, Vector<TopologyInfo::Bridge> *brigdes);
    static void get_info_backwd_packet(Packet *p, Vector<TopologyInfo::Bridge> *brigdes);

};

CLICK_ENDDECLS
#endif
