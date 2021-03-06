#ifndef NODECHANNELSTATS_HH
#define NODECHANNELSTATS_HH

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet.hh>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>

#include "elements/brn/brn2.h"
#include "elements/brn/wifi/rxinfo/channelstats/cooperativechannelstats_protocol.hh"

#define ENDIANESS_TEST 0x1234;

CLICK_DECLS

#define NODECHANNELSTATS_DEFAULT_SIZE 16

typedef HashMap<EtherAddress, struct neighbour_airtime_stats*> NeighbourStatsMap;
typedef NeighbourStatsMap::const_iterator NeighbourStatsMapIter;

class NodeChannelStats
{
  public:
    Timestamp _last_update;
  private:
    uint32_t _buffer_size;
    uint32_t _index;

    Vector<Packet*> _packet_buffer;          //packet from the neighbours
    NeighbourStatsMap *_nstats_buffer;       //infos, about the neighbours (list of maps). size of list is time backwards.

  public:
    EtherTimestampMap _last_neighbor_update; //time, when the node notice the neighbor last time
    uint32_t _used;

    explicit NodeChannelStats(uint32_t size) : _buffer_size(size), _index(0),
                                               _nstats_buffer(NULL), _last_neighbor_update(), _used(0)
    {
      _packet_buffer.resize(_buffer_size, NULL);
      _nstats_buffer = new NeighbourStatsMap[_buffer_size];
    }

    NodeChannelStats(const NodeChannelStats &ncs) : _buffer_size(ncs._buffer_size), _index(ncs._index),
                                                    _nstats_buffer(NULL), _last_neighbor_update(),_used(ncs._used)
    {
      _packet_buffer.resize(_buffer_size, NULL);
      _nstats_buffer = new NeighbourStatsMap[_buffer_size];
    }

    ~NodeChannelStats() {
      delete[] _nstats_buffer;
      for ( uint32_t i = 0; i < _buffer_size; i++) {
        if ( _packet_buffer[i] != NULL ) {
          _packet_buffer[i]->kill();
          _packet_buffer[i] = NULL;
        }
      }
    }

    struct local_airtime_stats *get_last_stats() {
       uint32_t prev_i = (_index + _buffer_size - 1) % _buffer_size;
      if ( _packet_buffer[prev_i] == NULL ) return NULL;
      return &(((struct cooperative_channel_stats_header*)_packet_buffer[prev_i]->data())->local_stats);
    }

    NeighbourStatsMap *get_last_neighbour_map() {
       uint32_t prev_i = (_index + _buffer_size - 1) % _buffer_size;
      if ( _packet_buffer[prev_i] == NULL ) return NULL;
      return &(_nstats_buffer[prev_i]);
    }

    bool insert(Packet *p) {
      _last_update = p->timestamp_anno();

      struct local_airtime_stats *ls = get_last_stats();
      struct cooperative_channel_stats_header *header = (struct cooperative_channel_stats_header*)p->data();
      struct local_airtime_stats *new_ls = &header->local_stats;

      struct cooperative_channel_stats_body *body = (struct cooperative_channel_stats_body *)&header[1];

      /* return if we already have this ID */
      if ( (ls != NULL) && ( ls->stats_id == new_ls->stats_id) ) return false;

      /* insert if new */
      if (_packet_buffer[_index] != NULL) {
        _packet_buffer[_index]->kill();
        _packet_buffer[_index] = NULL;
        _nstats_buffer[_index].clear();
      }

      _packet_buffer[_index] = p;

      struct neighbour_airtime_stats *nas = &(body->neighbour_stats);

      for ( uint8_t n = 0; n < header->no_neighbours; n++ ) {
        EtherAddress ea = EtherAddress(nas[n]._etheraddr);
        _nstats_buffer[_index].insert(ea, &nas[n]);
        _last_neighbor_update.insert(ea, _last_update);
      }

      _index = (_index + 1) % _buffer_size;
      if ( _used < _buffer_size ) _used++;

      return true;
    }

    int get_neighbours_with_max_age(int age, Vector<EtherAddress> *ea_vec) {
      int no_neighbours = 0;
      Timestamp now = Timestamp::now();
      for ( EtherTimestampMapIter nt_iter = _last_neighbor_update.begin(); nt_iter != _last_neighbor_update.end(); ++nt_iter ) {
        if ( (now - nt_iter.value()).msecval() < age ) {
          ea_vec->push_back(nt_iter.key());
          no_neighbours++;
        }
      }
      return no_neighbours;
    }
};

typedef HashMap<EtherAddress, NodeChannelStats*> NodeChannelStatsMap;
typedef NodeChannelStatsMap::const_iterator NodeChannelStatsMapIter;

CLICK_ENDDECLS
#endif	/* NODECHANNELSTATS_HH */

