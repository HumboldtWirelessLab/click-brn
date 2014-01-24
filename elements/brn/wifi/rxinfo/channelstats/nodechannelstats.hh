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

#include "elements/brn/wifi/rxinfo/channelstats/cooperativechannelstats_protocol.hh"

#define ENDIANESS_TEST 0x1234;

CLICK_DECLS

#define NODECHANNELSTATS_DEFAULT_SIZE 16

typedef HashMap<EtherAddress, struct neighbour_airtime_stats*> NeighbourStatsMap;
typedef NeighbourStatsMap::const_iterator NeighbourStatsMapIter;

class NodeChannelStats
{
  private:
    uint32_t _buffer_size;
    uint32_t _index;

    Packet **_packet_buffer;
    NeighbourStatsMap *_nstats_buffer;

  public:
    uint32_t _used;

    NodeChannelStats(uint32_t size) {
      _buffer_size = size;
      _index = 0;
      _used = 0;
      _packet_buffer = new Packet*[size];
      _nstats_buffer = new NeighbourStatsMap[size];
      memset(_packet_buffer, 0, size * sizeof(Packet*));
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
        _nstats_buffer[_index].insert(EtherAddress(nas[n]._etheraddr), &nas[n]);
      }

      _index = (_index + 1) % _buffer_size;
      if ( _used < _buffer_size ) _used++;

      return true;
    }
};

typedef HashMap<EtherAddress, NodeChannelStats*> NodeChannelStatsMap;
typedef NodeChannelStatsMap::const_iterator NodeChannelStatsMapIter;

CLICK_ENDDECLS
#endif	/* NODECHANNELSTATS_HH */

