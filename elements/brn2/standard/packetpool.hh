#ifndef PACKETPOOL_HH
#define PACKETPOOL_HH
#include <click/packet.hh>

CLICK_DECLS

class PacketPool {
 public:
  PacketPool(uint32_t capacity, uint32_t size_steps, uint32_t min_size, uint32_t max_size, uint32_t default_headroom, uint32_t default_tailroom);
  ~PacketPool();

  void insert(Packet *p);
  WritablePacket* get(uint32_t size, uint32_t headroom, uint32_t tailroom);

  String stats();

 private:
  uint32_t _capacity, _size_steps, _min_size, _max_size;
  uint32_t _default_headroom, _default_tailroom;

  uint32_t _no_sizes,_max_size_index;
  uint32_t *_packet_per_size;

  Packet **_packet_pool;

  uint32_t _inserts, _kills, _hits, _makes;
};

CLICK_ENDDECLS
#endif
