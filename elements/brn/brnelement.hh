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

#ifndef BRNELEMENT_H_
#define BRNELEMENT_H_

#include <click/element.hh>
// Brn Logger
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "elements/brn/standard/packetpool.hh"

#define PACKET_POOL_CAPACITY     10
#define PACKET_POOL_SIZE_STEPS  200
#define PACKET_POOL_MIN_SIZE    180
#define PACKET_POOL_MAX_SIZE   1600
#define DEFAULT_HEADROOM        128
#define DEFAULT_TAILROOM         32

CLICK_DECLS

class BRNElement : public Element {
 public:
  BRNElement();
  virtual ~BRNElement();
  virtual void add_handlers();

  void init(void);

  int _debug;

  static PacketPool *_packet_pool;
  static int _ref_counter;

  void packet_kill(Packet *p);
  WritablePacket *packet_new(uint32_t headroom, uint8_t *data, uint32_t size, uint32_t tailroom);

};

CLICK_ENDDECLS
#endif /*BRNELEMENT_H_*/
