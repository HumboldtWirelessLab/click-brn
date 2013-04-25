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
#include <elements/brn/brnelement.hh>
#include <elements/brn/wifi/channelstats.hh>
#include <elements/brn/wifi/collisioninfo.hh>
#include <elements/brn/wifi/packetlossinformation/packetlossinformation.hh>


CLICK_DECLS

/*
=c
()

=d

*/

#define BACKOFF_STRATEGY_OFF                             0 /* default */
#define BACKOFF_STRATEGY_NEIGHBOURS_PLI_AWARE            1 /* PLI:=PacketLossInformation */
#define BACKOFF_STRATEGY_NEIGHBOURS_CHANNEL_LOAD_AWARE   2
#define BACKOFF_STRATEGY_NEIGHBOURS_MAX_THROUGHPUT       3

class Tos2QueueMapper : public BRNElement {

 public:


    Tos2QueueMapper();
    ~Tos2QueueMapper();


    const char *class_name() const  { return "Tos2QueueMapper"; }
    const char *port_count() const  { return "1/1"; }
    const char *processing() const  { return AGNOSTIC;}

    int configure(Vector<String> &, ErrorHandler *);
	void add_handlers();
	Packet *simple_action(Packet *);//Function for the Agnostic Port

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
	
	void reset_queue_usage() { memset(_queue_usage, 0, sizeof(uint32_t) * no_queues); }
	
	void set_backoff_strategy(uint16_t value) { _bqs_strategy = value; }
	uint16_t get_backoff_strategy() { return _bqs_strategy; }
	
        int backoff_strategy_neighbours_pli_aware(Packet *p,uint8_t tos);

		uint16_t _bqs_strategy;//Backoff-Queue Selection Strategy(see above define declarations)

  private:
	ChannelStats *_cst; //Channel-Statistics-Element (see: ../channelstats.hh)
	CollisionInfo *_colinf;//Collision-Information-Element (see: ../collisioninfo.hh)
	PacketLossInformation *pli;//PacketLossInformation-Element (see:../packetlossinformation/packetlossinformation.hh)

	uint8_t no_queues;//number of queues
	uint16_t *_cwmin;//Contention Window Minimum; Array (see: monitor)
	uint16_t *_cwmax;//Contention Window Maximum; Array (see:monitor)
	uint16_t *_aifs;//Arbitration Inter Frame Space;Array (see 802.11e Wireless Lan for QoS)
	uint32_t *_queue_usage;//frequency of the used queues

        int32_t _likelihood_collison;
        int32_t _rate;
	int32_t _msdu_size;
	int32_t _index_packet_loss_max;
	int32_t _index_number_of_neighbours_max;
	int32_t _index_msdu_size_max;
	int32_t _index_rates_max;

};

CLICK_ENDDECLS
#endif
