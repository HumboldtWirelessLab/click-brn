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


CLICK_DECLS

/*
=c
()

=d

*/

#define BACKOFF_STRATEGY_OFF                             0 /* default */
#define BACKOFF_STRATEGY_MAX_THROUGHPUT                  1
#define BACKOFF_STRATEGY_CHANNEL_LOAD_AWARE              2
#define BACKOFF_STRATEGY_TARGET_PACKET_LOSS              3
#define BACKOFF_STRATEGY_LEARNING                        4

#define TOS2QM_DEFAULT_LEARNING_BO                       16

class Tos2QueueMapper : public BRNElement {

  public:

    Tos2QueueMapper();
    ~Tos2QueueMapper();

    const char *class_name() const  { return "Tos2QueueMapper"; }
    const char *port_count() const  { return "1/1"; }
    const char *processing() const  { return "a"; }

    int configure(Vector<String> &, ErrorHandler *);
    void add_handlers();
    
//    Packet * smaction(Packet *p, int port);
//    void push(int, Packet *);
//    Packet *pull(int);
    Packet *simple_action(Packet *p);

  
    void handle_feedback(Packet *);
    
    void set_backoff_strategy(uint16_t value) { _bqs_strategy = value; }
    uint16_t get_backoff_strategy() { return _bqs_strategy; }
    uint16_t _bqs_strategy;//Backoff-Queue Selection Strategy(see above define declarations)

    int backoff_strategy_neighbours_pli_aware(Packet *p, uint8_t tos);
    int backoff_strategy_channelload_aware(int /*busy*/, int /*nodes*/);
    int backoff_strategy_neighbours_aware(Packet *p, uint8_t tos);

    inline uint8_t get_no_queues() { return no_queues; }
    uint32_t get_queue_usage(uint8_t position);

    uint32_t get_cwmax(uint8_t position);
    uint32_t get_cwmin(uint8_t position);

    uint32_t get_learning_current_bo() { return _learning_current_bo; }
    uint32_t get_learning_count_up() { return _learning_count_up; }
    uint32_t get_learning_count_down() { return _learning_count_down; }
    
    int find_closest_size_index(int size);
    int find_closest_rate_index(int rate);
    int find_closest_no_neighbour_index(int no_neighbours);
    int find_closest_per_index(int per);
    int find_queue(int cwmin);

    void reset_queue_usage() { memset(_queue_usage, 0, sizeof(uint32_t) * no_queues); }

    
  private:
    uint32_t set_backoff();
    uint32_t get_backoff();
    ChannelStats *_cst;         //Channel-Statistics-Element (see: ../channelstats.hh)
    CollisionInfo *_colinf;     //Collision-Information-Element (see: ../collisioninfo.hh)

    uint32_t _learning_current_bo;
    uint32_t _learning_count_up;
    uint32_t _learning_count_down;
    uint32_t _learning_max_bo;
   
    uint8_t no_queues;          //number of queues
    uint16_t *_cwmin;           //Contention Window Minimum; Array (see: monitor)
    uint16_t *_cwmax;           //Contention Window Maximum; Array (see:monitor)
    uint16_t *_aifs;            //Arbitration Inter Frame Space;Array (see 802.11e Wireless Lan for QoS)
    uint32_t *_queue_usage;     //frequency of the used queues

    int32_t _likelihood_collison;

  public:
    uint32_t _feedback_cnt;
    uint32_t _tx_cnt;

};

CLICK_ENDDECLS
#endif
