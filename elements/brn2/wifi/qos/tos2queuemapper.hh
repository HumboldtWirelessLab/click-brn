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

#ifndef TOS2QUEUEMAPPERELEMENT_HH
#define TOS2QUEUEMAPPERELEMENT_HH
#include <click/element.hh>
#include <elements/brn2/brnelement.hh>
#include <elements/brn2/wifi/channelstats.hh>
#include <elements/brn2/wifi/collisioninfo.hh>
#include <elements/brn2/wifi/packetlossinformation/packetlossinformation.hh>


CLICK_DECLS

/*
=c
()

=d

*/

class Tos2QueueMapper : public BRNElement {

 public:
	#define BACKOFF_STRATEGY_ALWAYS_OFF 0 //default
	#define BACKOFF_STRATEGY_NEIGHBOURS_CHANNEL_LOAD_AWARE 2
	#define BACKOFF_STRATEGY_NEIGHBOURS_PLI_AWARE 1  //PLI:=PacketLossInformation


	Tos2QueueMapper();
	~Tos2QueueMapper();


	const char *class_name() const  { return "Tos2QueueMapper"; }
	const char *port_count() const  { return "1/1"; }

	int configure(Vector<String> &, ErrorHandler *);

	void add_handlers();

	Packet *simple_action(Packet *);
	void no_queues_set(uint8_t number);
	uint8_t no_queues_get();
	void queue_usage_set(uint8_t position, uint32_t value);
	uint32_t queue_usage_get(uint8_t position);
	uint32_t cwmax_get(uint8_t position);
	void cwmax_set(uint8_t position, uint32_t value);

	uint32_t cwmin_get(uint8_t position);
	void cwmin_set(uint8_t position, uint32_t value);
	int get_cwmin(int busy, int nodes);
	int find_queue(int cwmin);
	void reset_queue_usage() {
		memset(_queue_usage, 0, sizeof(uint32_t) * no_queues);
	  }
	void backoff_strategy_set(uint16_t value);
	uint16_t backoff_strategy_get();

  private:
	ChannelStats *_cst; //Channel-Statistics-Element (see: ../channelstats.hh)
	CollisionInfo *_colinf;//Collision-Information-Element (see: ../collisioninfo.hh)
	PacketLossInformation *pli;//PacketLossInformation-Element (see:../packetlossinformation/packetlossinformation.hh)

	uint8_t no_queues;//number of queues
	uint16_t *_cwmin;//Contention Window Minimum; Array (see: monitor)
	uint16_t *_cwmax;//Contention Window Maximum; Array (see:monitor)
	uint16_t *_aifs;//Arbitration Inter Frame Space;Array (see 802.11e Wireless Lan for QoS)
	uint16_t _bqs_strategy;//Backoff-Queue Selection Strategy(see above define declarations)
	uint32_t *_queue_usage;//frequency of the used queues

};

CLICK_ENDDECLS
#endif
